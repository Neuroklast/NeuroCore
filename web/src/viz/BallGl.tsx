import { hasJuceBridge } from "../bridge/juce";
import { useEffect, useRef, useState, type ReactNode } from "react";
import { particleAudio, PARTICLE_AUDIO_IDLE } from "./particleAudio";
import { STIR_STEP } from "./stirModel";
import {
  bandIndex,
  emptyForces,
  packForceUniforms,
  pushForce,
  spawnForce,
  stepForces,
} from "./forceField";
import { HASH, DUST_COMMON, STIR_FS } from "./particleShader";
import { cachedBands, publishViz } from "./vizSnap";
import { perfBegin, perfEnd, perfEnabled } from "../perf/marks";
import { noteDraw, noteProgram, noteResize, noteTexAlloc } from "../perf/counters";
import { createGpuTimer } from "../perf/gpuTimer";
import { useHostStore } from "../store/hostStore";
import { useTelemetryStore } from "../store/telemetryStore";
import { themeOf } from "../theme/theme";
import { subscribeVizClock } from "../theme/vizClock";
import { fitCanvas } from "./canvasFit";
import {
  BALL_CONFIG,
  MOTIONS,
  SHAPES,
  STATE_WIDTH,
  MAX_ARCS,
  ballCapable,
  ballDustCount,
  ballDrive,
  ballRadius,
  ballStill,
  buildDust,
  buildPalette,
  buildStrands,
  rgbTriplet,
  stereoWidth,
  type BallDrive,
} from "./ballModel";
import {
  CONTAINMENT_STATIC,
  containmentLayout,
  containmentPalette,
  containmentPose,
  publishCagePose,
  type ContainmentLayout,
  type ContainmentPalette,
  type ContainmentPose,
} from "./containmentModel";
import { createContainment } from "./containmentPass";
import { glOnLost, glOnRestored, glShouldDraw, type GlLife } from "./glLife";

const PIXEL_BUDGET = 4.5e6;
const INTRO_SECONDS = 2.2;


const PASS_VS = `#version 300 es
layout(location = 0) in vec2 position;
out vec2 vUv;
void main() {
  vUv = position * 0.5 + 0.5;
  gl_Position = vec4(position, 0.0, 1.0);
}
`;



const FIELD_FS = `#version 300 es
precision highp float;
uniform vec2 uCenter;
uniform float uRadius;
uniform float uDpr;
uniform float uLine;
uniform float uTime;
uniform float uFrame;
uniform float uBins;
uniform float uStrands;
uniform float uCrackle;
uniform float uFlares;
uniform float uGlow;
uniform float uHaze;
uniform float uFill;
uniform float uPresence;
uniform float uUnfold;
uniform float uBloom;
uniform float uInside;
uniform float uEncode;
uniform vec3 uRim;
uniform vec3 uRimHot;
uniform vec3 uRimMid;
uniform vec3 uRimDeep;
uniform vec3 uSpark;
uniform vec3 uSparkGlow;
uniform vec3 uHazeColor;
uniform vec3 uEdgeColor;
uniform vec4 uHeat;
uniform vec4 uHarmonics[8];
uniform vec4 uRates[8];
uniform vec4 uPhases[8];
uniform vec4 uFlareHarmonics[8];
uniform vec4 uFlareRates[8];
uniform vec4 uFlarePhases[8];
uniform vec4 uArcs[4];
out vec4 fragColor;
${HASH}
const float PI = 3.14159265;
const float TAU = 6.28318531;
const vec4 AMPS = vec4(0.35, 0.3, 0.22, 0.13);

float bell(float d, float w) { float x = d / w; return exp(-x * x); }

float pool(vec2 q) {
  float t = length(q - vec2(0.0, mix(-1.0, -0.66, uFill))) / mix(0.8, 1.35, uFill);
  return t < 0.6 ? mix(0.5, 0.22, t / 0.6) : mix(0.22, 0.0, clamp((t - 0.6) / 0.4, 0.0, 1.0));
}

void main() {
  vec2 p = gl_FragCoord.xy / uDpr - uCenter;
  float r = length(p);
  float R = uRadius;
  float theta = atan(p.y, p.x);
  float lift = p.y / max(r, 1e-3);
  float topDim = 1.0 - 0.35 * pow(max(lift, 0.0), 1.5);
  vec3 col = vec3(0.0);
  float hot = 0.0;
  uint frame = uint(uFrame);

  if (r < R) {
    vec2 q = p / R;
    float chord = sqrt(max(1.0 - dot(q, q), 0.0));
    float haze = pool(q) * (0.55 + 0.45 * chord);
    col += uHazeColor * haze * uHaze * uInside;
    float e = length(q - vec2(0.0, 0.2)) / 1.2;
    float edge = 0.5 * pow(smoothstep(0.62, 1.0, e), 1.4);
    col += uEdgeColor * edge * uInside * (0.5 + 0.5 * uHaze);
  }

  float dr = r - R;
  float halo = 1.0 - smoothstep(R * 0.05, R * 0.105, abs(dr - R * 0.0213));
  col += uRimDeep * 0.18 * halo * topDim * uGlow * uBloom;
  col += uRim * 0.12 * bell(dr + R * 0.0383, R * 0.032) * uGlow * uBloom;
  col += uRim * 0.32 * bell(dr, max(R * 0.0117, uLine)) * uPresence * uUnfold;

  float delta = mod(theta - uHeat.x + PI, TAU) - PI;
  float heat = uHeat.z * uHeat.y * exp(-delta * delta / 0.3);
  float crackle = uCrackle;
  float amp = R * 0.016 * (0.3 + 1.4 * crackle) * uUnfold;
  float jitterAmp = R * 0.0048 * (0.3 + 1.4 * uCrackle) * uUnfold;
  float flareAmp = R * 0.064 * (0.4 + 1.2 * crackle) * uFlares * 1.6 * uUnfold;

  if (abs(dr) < amp * 2.0 + flareAmp + R * 0.3) {
    float step = TAU / uBins;
    float binPos = (theta + PI) / step;
    float bin0 = floor(binPos);
    float binF = binPos - bin0;
    float th0 = bin0 * step - PI;
    float th1 = th0 + step;
    uint b0 = uint(mod(bin0, uBins));
    uint b1 = uint(mod(bin0 + 1.0, uBins));
    float gain = min(1.0, 2.4 / max(uStrands, 1.0));
    for (int k = 0; k < 8; k++) {
      if (float(k) >= uStrands) break;
      vec4 m = uHarmonics[k];
      vec4 lead = uRates[k] * uTime + uPhases[k];
      float wave0 = dot(AMPS, sin(m * th0 + lead));
      float wave1 = dot(AMPS, sin(m * th1 + lead));
      uint salt = uint(k) * 1013u + frame * 7919u;
      vec4 shape = uFlareHarmonics[k];
      vec4 look = uFlareRates[k];
      vec4 tone = uFlarePhases[k];
      float a = amp * shape.w;
      float off0 = a * wave0 + jitterAmp * (random(b0 + salt) - 0.5);
      float off1 = a * wave1 + jitterAmp * (random(b1 + salt) - 0.5);
      float rc = R + mix(off0, off1, binF);
      float grade = (off1 - off0) / (step * max(r, 1.0));
      float d = abs(r - rc) * inversesqrt(1.0 + grade * grade);
      float swell = 0.8 + 0.4 * (0.5 + 0.5 * sin(3.0 * theta + uTime * 0.7 + tone.x * 1.7));
      float width = uLine * look.w * swell;
      float core = exp(-d * d / (width * width));
      float sheath = exp(-d * d / (width * width * 3.0));
      float bright = tone.w * (1.0 + heat * 0.45) * uPresence;
      col += (uRimHot * core * 0.8 + uRim * sheath * 0.32) * bright;
      hot += core * bright;
      vec3 fargs = shape.xyz * theta + look.xyz * uTime + tone.xyz;
      float flare = max(0.0, (sin(fargs.x) + sin(fargs.y) + sin(fargs.z)) / 3.0);
      flare = (flare * flare) * flareAmp;
      float dw = abs(r - rc - flare);
      float dm = abs(r - rc - flare * 0.6);
      float dn = abs(r - rc - flare * 0.25);
      vec3 glow = uRimDeep * (0.07 * bell(dw, R * 0.09) + 0.16 * bell(dw, R * 0.05));
      glow += uRimMid * 0.28 * bell(dm, R * 0.027) + uRim * 0.38 * bell(dn, R * 0.013);
      col += glow * gain * topDim * uGlow * uBloom * (1.0 + heat * 1.4);
    }
  }

  for (int i = 0; i < 4; i++) {
    vec4 arc = uArcs[i];
    if (arc.w <= 0.002) continue;
    float mid = arc.x + arc.y * 0.5;
    vec2 center = R * 0.93 * vec2(cos(mid), sin(mid));
    if (distance(p, center) > R * (abs(arc.y) * 0.6 + 0.1) + 12.0) continue;
    float ar = R * 0.955;
    vec2 a0 = ar * vec2(cos(arc.x), sin(arc.x));
    vec2 a2 = ar * vec2(cos(arc.x + arc.y), sin(arc.x + arc.y));
    vec2 a1 = R * (0.955 - arc.z) * vec2(cos(mid), sin(mid));
    float dmin = 1e5;
    float along = 0.0;
    vec2 prev = a0;
    for (int s = 1; s <= 10; s++) {
      float u = float(s) / 10.0;
      vec2 b = mix(mix(a0, a1, u), mix(a1, a2, u), u);
      vec2 pa = p - prev;
      vec2 ba = b - prev;
      float h = clamp(dot(pa, ba) / max(dot(ba, ba), 1e-5), 0.0, 1.0);
      float dist = length(pa - ba * h);
      if (dist < dmin) { dmin = dist; along = (float(s) - 1.0 + h) / 10.0; }
      prev = b;
    }
    float taper = sqrt(max(sin(PI * along), 0.0));
    float arcWidth = uLine * (0.55 + 0.6 * taper);
    float arcCore = exp(-dmin * dmin / (arcWidth * arcWidth));
    float arcGlow = exp(-dmin * dmin / (arcWidth * arcWidth * 12.0));
    col += (uSpark * 0.95 * arcCore + uSparkGlow * 0.4 * arcGlow) * arc.w * taper;
    hot += arcCore * arc.w * taper * 0.6;
  }

  fragColor = vec4(col, hot) * uEncode;
}
`;

const DUST_VS = `#version 300 es
precision highp float;
layout(location = 0) in float aIndex;
uniform sampler2D tHome;
uniform sampler2D tSeed;
uniform sampler2D tOffset;
uniform float uStirred;
uniform vec2 uCenter;
uniform vec2 uViewport;
uniform float uRadius;
uniform float uDpr;
uniform float uDepth;
uniform float uTwinkle;
uniform float uPointScale;
uniform float uReveal;
uniform float uLift;
uniform float uEncode;
uniform vec3 uTones[5];
uniform vec3 uRimTone;
out vec3 vColor;
out float vSize;
${HASH}
${DUST_COMMON}
void main() {
  ivec2 cell = ivec2(int(mod(aIndex, ${STATE_WIDTH}.0)), int(floor(aIndex / ${STATE_WIDTH}.0)));
  uint id = uint(aIndex);
  vec4 home = texelFetch(tHome, cell, 0);
  vec4 seed = texelFetch(tSeed, cell, 0);
  float life;
  float spread;
  vec3 p = drift(home, seed, id, life, spread);
  if (uStirred > 0.5) p += texelFetch(tOffset, cell, 0).xyz;

  float len = length(p);
  float inside = 1.0 - smoothstep(0.955, 0.985, length(p.xy));
  if (len > 0.975) p *= 0.975 / len;

  float front = p.z * 0.5 + 0.5;
  float depthSize = mix(1.0, 0.7 + 0.6 * front, uDepth);
  float depthLight = mix(1.0, 0.35 + 0.8 * front, uDepth);
  float blink = sin(uDustTime * (2.0 + 6.0 * random(id * 7u + 5u)) + random(id * 7u + 6u) * 6.2831853);
  float twinkle = mix(1.0, smoothstep(-0.9, -0.6, blink), uTwinkle);
  float fade = smoothstep(0.04, 0.12, life);
  float order = (home.y + 1.0) * 0.5 * 0.55 + random(id * 7u + 8u) * 0.25;
  float reveal = smoothstep(order, order + 0.2, uReveal);
  float alpha = random(id * 7u + 9u) < 0.5 || home.w > 3.5 ? 1.0 : 0.6;

  float size = seed.x * uPointScale * (0.35 + 0.95 * life) * depthSize * mix(0.5, 1.0, reveal);
  vec2 screen = uCenter + p.xy * uRadius;
  gl_Position = vec4(screen / uViewport * 2.0 - 1.0, 0.0, 1.0);
  float px = size * uDpr;
  gl_PointSize = px + 2.0;
  vSize = px;

  int tone = int(home.w + 0.5);
  float rimLit = smoothstep(0.8, 0.97, length(p.xy));
  vec3 color = uTones[tone] + uRimTone * rimLit * rimLit * 0.12;
  vColor = color * alpha * fade * twinkle * depthLight * reveal * inside * sqrt(spread) * uEncode;
}
`;

const DUST_FS = `#version 300 es
precision highp float;
uniform float uShape;
in vec3 vColor;
in float vSize;
out vec4 fragColor;
void main() {
  vec2 q = (gl_PointCoord - 0.5) * (vSize + 2.0);
  float half_ = max(vSize * 0.5, 0.5);
  float cover;
  if (uShape < 0.5) {
    vec2 c = clamp(half_ - abs(q) + 0.5, 0.0, 1.0);
    cover = c.x * c.y;
  } else {
    cover = clamp(half_ - length(q) + 0.5, 0.0, 1.0);
  }
  if (cover <= 0.0) discard;
  fragColor = vec4(vColor * cover, 0.0);
}
`;


const COMPOSITE_FS = `#version 300 es
precision highp float;
uniform sampler2D tScene;
uniform float uLight;
uniform float uDecode;
in vec2 vUv;
out vec4 fragColor;
vec3 soften(vec3 x) {
  vec3 over = max(x - 0.6, 0.0);
  return min(x, 0.6) + 0.4 * (1.0 - exp(-over / 0.4));
}
void main() {
  vec4 scene = texture(tScene, vUv) * uDecode;
  vec3 light = max(scene.rgb, 0.0);
  float peak = max(light.r, max(light.g, light.b));
  if (uLight > 0.5) {
    vec3 hue = light / max(peak, 1e-4);
    float cover = 1.0 - exp(-peak * 2.4);
    vec3 ink = hue * mix(0.92, 0.72, cover);
    float core = clamp(scene.a * 0.7, 0.0, 1.0) * cover;
    vec3 tint = mix(vec3(1.0), hue, 0.22);
    fragColor = vec4(tint * core + ink * cover * (1.0 - core), core + cover * (1.0 - core));
    return;
  }
  light += vec3(max(peak - 1.8, 0.0) * 0.25);
  vec3 shown = soften(light);
  fragColor = vec4(shown, max(shown.r, max(shown.g, shown.b)));
}
`;

function buildProgram(gl: WebGL2RenderingContext, vsSrc: string, fsSrc: string): WebGLProgram | null {
  const compile = (type: number, src: string) => {
    const s = gl.createShader(type);
    if (! s) {
      return null;
    }
    gl.shaderSource(s, src);
    gl.compileShader(s);
    if (! gl.getShaderParameter(s, gl.COMPILE_STATUS)) {
      gl.deleteShader(s);
      return null;
    }
    return s;
  };
  const vs = compile(gl.VERTEX_SHADER, vsSrc);
  const fs = compile(gl.FRAGMENT_SHADER, fsSrc);
  if (! vs || ! fs) {
    return null;
  }
  const prog = gl.createProgram();
  if (! prog) {
    return null;
  }
  gl.attachShader(prog, vs);
  gl.attachShader(prog, fs);
  gl.linkProgram(prog);
  gl.deleteShader(vs);
  gl.deleteShader(fs);
  if (! gl.getProgramParameter(prog, gl.LINK_STATUS)) {
    gl.deleteProgram(prog);
    return null;
  }
  return prog;
}

type Target = { fb: WebGLFramebuffer; textures: WebGLTexture[]; w: number; h: number; ok: boolean };

function makeTexture(
  gl: WebGL2RenderingContext,
  w: number,
  h: number,
  internalFormat: number,
  type: number,
  data: ArrayBufferView | null = null,
): WebGLTexture {
  noteTexAlloc();
  const tex = gl.createTexture() as WebGLTexture;
  gl.bindTexture(gl.TEXTURE_2D, tex);
  gl.texImage2D(gl.TEXTURE_2D, 0, internalFormat, w, h, 0, gl.RGBA, type, data);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.NEAREST);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.NEAREST);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
  return tex;
}

function makeTarget(
  gl: WebGL2RenderingContext,
  w: number,
  h: number,
  colors: number,
  internalFormat: number,
  type: number,
): Target {
  const fb = gl.createFramebuffer() as WebGLFramebuffer;
  gl.bindFramebuffer(gl.FRAMEBUFFER, fb);
  const textures: WebGLTexture[] = [];
  const attachments: number[] = [];
  for (let i = 0; i < colors; i += 1) {
    const tex = makeTexture(gl, w, h, internalFormat, type);
    gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0 + i, gl.TEXTURE_2D, tex, 0);
    textures.push(tex);
    attachments.push(gl.COLOR_ATTACHMENT0 + i);
  }
  gl.drawBuffers(attachments);
  const ok = gl.checkFramebufferStatus(gl.FRAMEBUFFER) === gl.FRAMEBUFFER_COMPLETE;
  gl.bindFramebuffer(gl.FRAMEBUFFER, null);
  return { fb, textures, w, h, ok };
}

type Arc = { start: number; span: number; curl: number; life: number; duration: number; drift: number; strength: number };

/**
 * Unit-face ball. Glass rim with crackling strands and arcs, plus a GPU particle state
 * (offset + velocity) stirred by gonio width, bass, transients and loudness. Theme accent, no white.
 * Without WebGL2, or on shader failure, `fallback` (the legacy wordmark + analyzer) renders.
 */
export function BallGl({ fallback }: { fallback: ReactNode }) {
  const ref = useRef<HTMLCanvasElement>(null);
  const [failed, setFailed] = useState(false);
  const [epoch, setEpoch] = useState(0);
  const skipLose = useRef(false);
  const brushOn = typeof window !== "undefined"
    && /(?:^|[?&])nkbrush=1(?:&|$)/.test(window.location.search);

  useEffect(() => {
    const canvas = ref.current;
    if (! canvas) {
      return undefined;
    }
    const get = (id: string) => canvas.getContext(id as "webgl2") as WebGL2RenderingContext | null;
    if (! ballCapable(get)) {
      setFailed(true);
      return undefined;
    }
    const gl = canvas.getContext("webgl2", { alpha: true, premultipliedAlpha: true, antialias: false, depth: false });
    if (! gl) {
      setFailed(true);
      return undefined;
    }

    const field = buildProgram(gl, PASS_VS, FIELD_FS);
    const dust = buildProgram(gl, DUST_VS, DUST_FS);
    const composite = buildProgram(gl, PASS_VS, COMPOSITE_FS);
    const fullFloat = !!gl.getExtension("EXT_color_buffer_float");
    const halfFloat = fullFloat || !!gl.getExtension("EXT_color_buffer_half_float");
    const stir = halfFloat ? buildProgram(gl, PASS_VS, STIR_FS) : null;
    if (! field || ! dust || ! composite || (halfFloat && !stir)) {
      setFailed(true);
      return undefined;
    }
    const containment = createContainment(gl);
    if (gl.getError() !== gl.NO_ERROR) {
      setFailed(true);
      return undefined;
    }
    const encode = halfFloat ? 1 : 0.25;

    const locations = (prog: WebGLProgram, names: string[]) => {
      const out: Record<string, WebGLUniformLocation | null> = {};
      for (const n of names) {
        out[n] = gl.getUniformLocation(prog, n);
      }
      return out;
    };
    const fieldU = locations(field, [
      "uCenter", "uRadius", "uDpr", "uLine", "uTime", "uFrame", "uBins", "uStrands",
      "uCrackle", "uFlares", "uGlow", "uHaze", "uFill", "uPresence", "uUnfold", "uBloom", "uInside", "uEncode",
      "uRim", "uRimHot", "uRimMid", "uRimDeep", "uSpark", "uSparkGlow", "uHazeColor", "uEdgeColor", "uHeat",
      "uHarmonics[0]", "uRates[0]", "uPhases[0]", "uFlareHarmonics[0]", "uFlareRates[0]", "uFlarePhases[0]", "uArcs[0]",
    ]);
    const dustU = locations(dust, [
      "tHome", "tSeed", "tOffset", "uStirred", "uCenter", "uViewport", "uRadius", "uDpr",
      "uDepth", "uTwinkle", "uPointScale", "uReveal", "uLift", "uEncode", "uTones[0]", "uRimTone", "uShape",
      "uDustTime", "uTurn", "uMotion", "uFill",
    ]);
    const stirU = stir
      ? locations(stir, ["tHome", "tSeed", "tOffset", "tVelocity", "uDt", "uReset", "uSpread", "uLiftForce", "uSwirl", "uDustTime", "uTurn", "uMotion", "uFill", "uForcePos[0]", "uForceKick[0]"])
      : {};
    const compositeU = locations(composite, ["tScene", "uLight", "uDecode"]);

    const strands = buildStrands();
    const harmonics = new Float32Array(strands.harmonics);
    const rates = new Float32Array(strands.rates);
    const phases = new Float32Array(strands.phases);
    const flareHarmonics = new Float32Array(strands.flareHarmonics);
    const flareRates = new Float32Array(strands.flareRates);
    const flarePhases = new Float32Array(strands.flarePhases);
    const arcs = new Float32Array(MAX_ARCS * 4);

    const quadVao = gl.createVertexArray();
    const quadBuf = gl.createBuffer();
    gl.bindVertexArray(quadVao);
    gl.bindBuffer(gl.ARRAY_BUFFER, quadBuf);
    gl.bufferData(gl.ARRAY_BUFFER, new Float32Array([-1, -1, 3, -1, -1, 3]), gl.STATIC_DRAW);
    gl.enableVertexAttribArray(0);
    gl.vertexAttribPointer(0, 2, gl.FLOAT, false, 0, 0);

    const dustVao = gl.createVertexArray();
    const dustBuf = gl.createBuffer();
    gl.bindVertexArray(dustVao);
    gl.bindBuffer(gl.ARRAY_BUFFER, dustBuf);
    gl.enableVertexAttribArray(0);
    gl.vertexAttribPointer(0, 1, gl.FLOAT, false, 0, 0);
    gl.bindVertexArray(null);

    const blank = makeTexture(gl, 1, 1, gl.RGBA8, gl.UNSIGNED_BYTE);

    const sceneFormat = halfFloat ? gl.RGBA16F : gl.RGBA8;
    const sceneType = halfFloat ? gl.HALF_FLOAT : gl.UNSIGNED_BYTE;
    const stateFormat = fullFloat ? gl.RGBA32F : gl.RGBA16F;
    const stateType = fullFloat ? gl.FLOAT : gl.HALF_FLOAT;

    let scene = makeTarget(gl, 1, 1, 1, sceneFormat, sceneType);
    if (! scene.ok) {
      setFailed(true);
      return undefined;
    }
    const gpu = perfEnabled() ? createGpuTimer(gl) : null;
    const drawArrays = (mode: number, first: number, count: number) => {
      noteDraw();
      gl.drawArrays(mode, first, count);
    };
    let fieldStatics = false;
    let fieldLayoutKey = "";
    let fieldThemeId = "";
    let dustStatics = false;
    let stirStatics = false;
    let homeTex: WebGLTexture | null = null;
    let seedTex: WebGLTexture | null = null;
    let stateRead: Target | null = null;
    let stateWrite: Target | null = null;
    let dustCount = 0;
    let requested = 0;

    const disposeTarget = (t: Target | null) => {
      if (! t) {
        return;
      }
      for (const tex of t.textures) {
        gl.deleteTexture(tex);
      }
      gl.deleteFramebuffer(t.fb);
    };
    const disposeDust = () => {
      if (homeTex) {
        gl.deleteTexture(homeTex);
        homeTex = null;
      }
      if (seedTex) {
        gl.deleteTexture(seedTex);
        seedTex = null;
      }
      disposeTarget(stateRead);
      disposeTarget(stateWrite);
      stateRead = null;
      stateWrite = null;
    };

    const resetStir = () => {
      if (! stir || ! stateRead || ! stateWrite) {
        return;
      }
      stirStatics = false;
      gl.useProgram(stir);
      gl.uniform1i(stirU.tHome, 0);
      gl.uniform1i(stirU.tSeed, 1);
      gl.uniform1i(stirU.tOffset, 4);
      gl.uniform1i(stirU.tVelocity, 5);
      gl.uniform1f(stirU.uReset, 1);
      // Every sampler must point at a bound texture, or the draw is rejected.
      gl.activeTexture(gl.TEXTURE0);
      gl.bindTexture(gl.TEXTURE_2D, blank);
      gl.activeTexture(gl.TEXTURE1);
      gl.bindTexture(gl.TEXTURE_2D, blank);
      gl.activeTexture(gl.TEXTURE4);
      gl.bindTexture(gl.TEXTURE_2D, blank);
      gl.activeTexture(gl.TEXTURE5);
      gl.bindTexture(gl.TEXTURE_2D, blank);
      gl.disable(gl.BLEND);
      gl.bindVertexArray(quadVao);
      gl.bindFramebuffer(gl.FRAMEBUFFER, stateRead.fb);
      gl.viewport(0, 0, stateRead.w, stateRead.h);
      drawArrays(gl.TRIANGLES, 0, 3);
      gl.bindFramebuffer(gl.FRAMEBUFFER, stateWrite.fb);
      drawArrays(gl.TRIANGLES, 0, 3);
      gl.bindFramebuffer(gl.FRAMEBUFFER, null);
      gl.bindVertexArray(null);
    };

    const rebuildDust = (count: number) => {
      const total = Math.max(0, Math.min(40000, Math.round(count / 100) * 100));
      if (total === requested) {
        return;
      }
      requested = total;
      disposeDust();
      if (! total) {
        dustCount = 0;
        return;
      }
      const built = buildDust(total);
      dustCount = built.count;
      homeTex = makeTexture(gl, STATE_WIDTH, built.rows, gl.RGBA32F, gl.FLOAT, built.home);
      seedTex = makeTexture(gl, STATE_WIDTH, built.rows, gl.RGBA32F, gl.FLOAT, built.seed);
      const indices = new Float32Array(built.count);
      for (let i = 0; i < built.count; i += 1) {
        indices[i] = i;
      }
      gl.bindVertexArray(dustVao);
      gl.bindBuffer(gl.ARRAY_BUFFER, dustBuf);
      gl.bufferData(gl.ARRAY_BUFFER, indices, gl.STATIC_DRAW);
      gl.bindVertexArray(null);
      if (stir) {
        stateRead = makeTarget(gl, STATE_WIDTH, built.rows, 2, stateFormat, stateType);
        stateWrite = makeTarget(gl, STATE_WIDTH, built.rows, 2, stateFormat, stateType);
      }
      resetStir();
    };

    const reduced = typeof window !== "undefined"
      && window.matchMedia?.("(prefers-reduced-motion: reduce)").matches === true;

    let width = 1;
    let height = 1;
    let dpr = 1;
    let themeId = "";
    let accent = rgbTriplet("196, 42, 42");
    let cyan = rgbTriplet("62, 200, 224");
    let palette = buildPalette(accent, cyan);
    let tones = new Float32Array(palette.tones.flat());
    let drive: BallDrive | null = null;
    let cagePose: ContainmentPose = CONTAINMENT_STATIC;
    let rimTime = 0;
    let dustTime = 0;
    let swayClock = 0;
    let introClock = 0;
    let audioState = PARTICLE_AUDIO_IDLE;
    let forces = emptyForces();
    let forceId = 1;
    const brush = typeof window !== "undefined"
      && /(?:^|[?&])nkbrush=1(?:&|$)/.test(window.location.search);
    let last = 0;
    let lastScreenW = 0;
    let lastPixelRatio = 0;
    let lastW = 0;
    let lastH = 0;
    let stillDrawn = false;
    let cageLayout: ContainmentLayout | null = null;
    let cageLayoutW = 0;
    let cageLayoutH = 0;
    let cagePalette: ContainmentPalette = containmentPalette(themeOf("signal"));

    let alive = true;
    const liveArcs: Arc[] = [];

    let arcRandom = 913;
    const arcRoll = () => {
      arcRandom = (arcRandom * 1664525 + 1013904223) >>> 0;
      return arcRandom / 4294967296;
    };

    const spawnArc = (angle: number, strength: number) => {
      if (liveArcs.length >= MAX_ARCS) {
        return;
      }
      liveArcs.push({
        start: angle,
        span: (0.05 + arcRoll() * 0.07) * (arcRoll() < 0.5 ? -1 : 1),
        curl: 0.02 + arcRoll() * 0.04,
        life: 0,
        duration: 0.2 + arcRoll() * 0.2,
        drift: (arcRoll() - 0.5) * 0.3,
        strength,
      });
    };

    const frame = (now: number) => {
      if (! alive || ! glShouldDraw(life)) {
        return;
      }
      gpu?.poll();
      const host = useHostStore.getState();
      const still = ballStill(host.motion);
      width = Math.max(1, canvas.clientWidth);
      height = Math.max(1, canvas.clientHeight);
      if (width <= 1 || height <= 1) {
        return;
      }
      const pixelRatio = window.devicePixelRatio || 1;
      const screenW = canvas.getBoundingClientRect().width;
      const sizeChanged = width !== lastW || height !== lastH || screenW !== lastScreenW || pixelRatio !== lastPixelRatio || themeId !== host.theme;
      if (still && stillDrawn && ! sizeChanged) {
        return;
      }
      lastW = width;
      lastH = height;
      lastScreenW = screenW;
      lastPixelRatio = pixelRatio;
      perfBegin("ball");
      const dt = last > 0 ? Math.min(0.05, Math.max(1 / 240, (now - last) / 1000)) : 1 / 60;
      last = now;
      introClock = still ? 1 : Math.min(1, introClock + dt / INTRO_SECONDS);
      stillDrawn = still;

      if (themeId !== host.theme) {
        themeId = host.theme;
        const themePalette = themeOf(themeId);
        accent = rgbTriplet(themePalette.accentRgb);
        cyan = rgbTriplet(themePalette.cyanRgb);
        palette = buildPalette(accent, cyan);
        tones = new Float32Array(palette.tones.flat());
        cagePalette = containmentPalette(themePalette);
        fieldThemeId = "";
      }

      const telemetry = useTelemetryStore.getState();
      const rate = host.sr > 0 ? host.sr : 48000;
      const live = telemetry.available || !hasJuceBridge();
      const bands = cachedBands(telemetry.tick, telemetry.scopeOut, rate, live).bands;
      const hit = particleAudio(audioState, {
        tick: telemetry.tick,
        seq: telemetry.seq,
        bass: bands.low,
        mid: bands.mid,
        high: bands.high,
        live,
        onsetCount: telemetry.onsetCount, onsetAmp: telemetry.onsetAmp,
        onsets: telemetry.onsets,
      }, dt);
      audioState = hit.state;
      if (hit.spawns.length > 0) {
        for (const spawn of hit.spawns) {
          forces = pushForce(forces, spawnForce(forceId, bandIndex(spawn.band), spawn.balance, spawn.amp));
          forceId += 1;
        }
      }
      const width01 = stereoWidth(telemetry.gonioL, telemetry.gonioR);
      drive = ballDrive(drive, bands, telemetry.outRms, width01, dt, {
        motion: host.motion,
        prefersReduced: reduced,
        onset: hit.impulse,
      });
      const d = drive;

      const radius = ballRadius(width, height);
      const presence = 1 - Math.pow(1 - Math.min(1, Math.max(0, introClock / 0.42)), 3);
      const unfold = 1 - Math.pow(1 - Math.min(1, Math.max(0, introClock / 0.32)), 3);
      const inside = Math.min(1, Math.max(0, (introClock - 0.2) / 0.75));

      if (! still) {
        const step = dt * BALL_CONFIG.speed;
        rimTime += step;
        dustTime += dt * d.dustSpeed;
        swayClock += dt * Math.max(0.15, d.dustSpeed * 0.6);
        if (introClock > 0.55 && d.impulse > 0.18) {
          spawnArc(arcRoll() * Math.PI * 2, 0.4 + 0.5 * Math.max(d.impulse, d.kick));
        }
        for (let i = liveArcs.length - 1; i >= 0; i -= 1) {
          const arc = liveArcs[i]!;
          arc.life += step;
          arc.start += arc.drift * step;
          if (arc.life >= arc.duration) {
            liveArcs.splice(i, 1);
          }
        }
      } else {
        liveArcs.length = 0;
      }
      for (let i = 0; i < MAX_ARCS; i += 1) {
        const arc = liveArcs[i];
        arcs[i * 4] = arc ? arc.start : 0;
        arcs[i * 4 + 1] = arc ? arc.span : 0;
        arcs[i * 4 + 2] = arc ? arc.curl : 0;
        arcs[i * 4 + 3] = arc ? Math.sin((Math.PI * arc.life) / arc.duration) * arc.strength : 0;
      }

      dpr = Math.min(
        typeof window !== "undefined" ? window.devicePixelRatio || 1 : 1,
        2,
        Math.sqrt(PIXEL_BUDGET / (width * height)),
      );
      const hostScale = screenW > 0 ? screenW / width : 1;
      const fit = fitCanvas(canvas, dpr, PIXEL_BUDGET, hostScale);
      dpr = fit.scale;
      const devW = canvas.width;
      const devH = canvas.height;
      if (scene.w !== devW || scene.h !== devH) {
        noteResize();
        disposeTarget(scene);
        scene = makeTarget(gl, devW, devH, 1, sceneFormat, sceneType);
        if (! scene.ok) {
          setFailed(true);
          perfEnd("ball");
          return;
        }
      }

      rebuildDust(ballDustCount(width, height));

      const turnX = BALL_CONFIG.sway * 0.42 * Math.sin(swayClock * 0.23);
      const turnY = BALL_CONFIG.sway * 0.12 * Math.sin(swayClock * 0.17 + 1.3);

      gl.disable(gl.DEPTH_TEST);
      gl.depthMask(false);
      gl.bindFramebuffer(gl.FRAMEBUFFER, scene.fb);
      gl.viewport(0, 0, devW, devH);
      gl.disable(gl.BLEND);
      gl.clearColor(0, 0, 0, 0);
      gl.clear(gl.COLOR_BUFFER_BIT);

      if (containment && (cageLayoutW !== width || cageLayoutH !== height || ! cageLayout)) {
        cageLayoutW = width;
        cageLayoutH = height;
        cageLayout = containmentLayout(width, height);
      }
      const cLayout = containment ? cageLayout : null;
      if (containment) {
        cagePose = still
          ? CONTAINMENT_STATIC
          : containmentPose(cagePose, d, dt, rimTime, host.motion);
        publishCagePose(cagePose);
      }
      publishViz(d, cagePose, telemetry.tick);
      const cArgs = cLayout
        ? {
            layout: cLayout,
            pose: cagePose,
            palette: cagePalette,
            dpr,
            encode,
          }
        : null;
      if (containment && cArgs) {
        gpu?.begin("contain-back");
        containment.bind(cArgs);
        gl.enable(gl.BLEND);
        gl.blendFuncSeparate(gl.SRC_ALPHA, gl.ONE_MINUS_SRC_ALPHA, gl.ONE, gl.ONE_MINUS_SRC_ALPHA);
        containment.draw(0, "metal");
        gl.blendFunc(gl.ONE, gl.ONE);
        containment.draw(0, "energy");
        gpu?.end();
      }

      gl.enable(gl.BLEND);
      gl.blendFunc(gl.ONE, gl.ONE);
      gl.useProgram(field);
      noteProgram();
      if (! fieldStatics) {
        gl.uniform1f(fieldU.uStrands, BALL_CONFIG.strands);
        gl.uniform1f(fieldU.uCrackle, BALL_CONFIG.crackle);
        gl.uniform1f(fieldU.uFlares, BALL_CONFIG.flares);
        gl.uniform1f(fieldU.uHaze, BALL_CONFIG.haze);
        gl.uniform1f(fieldU.uFill, BALL_CONFIG.fill);
        gl.uniform4fv(fieldU["uHarmonics[0]"] as WebGLUniformLocation, harmonics);
        gl.uniform4fv(fieldU["uRates[0]"] as WebGLUniformLocation, rates);
        gl.uniform4fv(fieldU["uPhases[0]"] as WebGLUniformLocation, phases);
        gl.uniform4fv(fieldU["uFlareHarmonics[0]"] as WebGLUniformLocation, flareHarmonics);
        gl.uniform4fv(fieldU["uFlareRates[0]"] as WebGLUniformLocation, flareRates);
        gl.uniform4fv(fieldU["uFlarePhases[0]"] as WebGLUniformLocation, flarePhases);
        fieldStatics = true;
      }
      const nextFieldLayout = `${width}|${height}|${dpr}|${radius}|${encode}`;
      if (nextFieldLayout !== fieldLayoutKey) {
        fieldLayoutKey = nextFieldLayout;
        gl.uniform2f(fieldU.uCenter, width / 2, height / 2);
        gl.uniform1f(fieldU.uRadius, radius);
        gl.uniform1f(fieldU.uDpr, dpr);
        gl.uniform1f(fieldU.uLine, Math.max(0.6, radius * 0.0035));
        gl.uniform1f(fieldU.uBins, Math.max(240, Math.min(900, Math.round((Math.PI * 2 * radius) / 2.6))));
        gl.uniform1f(fieldU.uEncode, encode);
      }
      if (fieldThemeId !== themeId) {
        fieldThemeId = themeId;
        gl.uniform3fv(fieldU.uRim, palette.rim);
        gl.uniform3fv(fieldU.uRimHot, palette.rimHot);
        gl.uniform3fv(fieldU.uRimMid, palette.rimMid);
        gl.uniform3fv(fieldU.uRimDeep, palette.rimDeep);
        gl.uniform3fv(fieldU.uSpark, palette.spark);
        gl.uniform3fv(fieldU.uSparkGlow, palette.sparkGlow);
        gl.uniform3fv(fieldU.uHazeColor, palette.haze);
        gl.uniform3fv(fieldU.uEdgeColor, palette.edge);
      }
      gl.uniform1f(fieldU.uTime, rimTime);
      gl.uniform1f(fieldU.uFrame, Math.floor(rimTime * 24) % 100000);
      gl.uniform1f(fieldU.uGlow, BALL_CONFIG.glow * (0.55 + 0.9 * d.glow));
      gl.uniform1f(fieldU.uPresence, presence);
      gl.uniform1f(fieldU.uUnfold, unfold);
      gl.uniform1f(fieldU.uBloom, presence * presence);
      gl.uniform1f(fieldU.uInside, inside);
      gl.uniform4f(
        fieldU.uHeat,
        (rimTime * 0.9) % (Math.PI * 2),
        d.kick,
        0.35 + 0.65 * d.kick,
        d.lift,
      );
      gl.uniform4fv(fieldU["uArcs[0]"] as WebGLUniformLocation, arcs);
      gl.bindVertexArray(quadVao);
      gpu?.begin("field");
      drawArrays(gl.TRIANGLES, 0, 3);
      gpu?.end();

      if (stir && stateRead && stateWrite && ! still) {
        const steps = Math.max(1, Math.ceil(Math.min(dt, 0.05) / STIR_STEP));
        const h = Math.min(dt, 0.05) / steps;
        let read = stateRead;
        let write = stateWrite;
        gl.useProgram(stir);
        noteProgram();
        if (! stirStatics) {
          gl.uniform1i(stirU.tHome, 0);
          gl.uniform1i(stirU.tSeed, 1);
          gl.uniform1i(stirU.tOffset, 2);
          gl.uniform1i(stirU.tVelocity, 3);
          gl.uniform1f(stirU.uMotion, MOTIONS[BALL_CONFIG.motion] ?? 0);
          gl.uniform1f(stirU.uFill, BALL_CONFIG.fill);
          stirStatics = true;
        }
        gl.uniform1f(stirU.uDt, h);
        gl.uniform1f(stirU.uReset, 0);
        gl.uniform1f(stirU.uSpread, d.spread);
        gl.uniform1f(stirU.uLiftForce, d.lift);
        gl.uniform1f(stirU.uSwirl, d.swirl);
        gl.uniform1f(stirU.uDustTime, dustTime);
        gl.uniform2f(stirU.uTurn, turnX, turnY);
        const packed = packForceUniforms(forces);
        gl.uniform4fv(stirU["uForcePos[0]"] as WebGLUniformLocation, packed.pos);
        gl.uniform4fv(stirU["uForceKick[0]"] as WebGLUniformLocation, packed.kick);
        gl.activeTexture(gl.TEXTURE0);
        gl.bindTexture(gl.TEXTURE_2D, homeTex);
        gl.activeTexture(gl.TEXTURE1);
        gl.bindTexture(gl.TEXTURE_2D, seedTex);
        gl.disable(gl.BLEND);
        gl.bindVertexArray(quadVao);
        gpu?.begin("stir");
        for (let s = 0; s < steps; s += 1) {
          gl.activeTexture(gl.TEXTURE2);
          gl.bindTexture(gl.TEXTURE_2D, read.textures[0]!);
          gl.activeTexture(gl.TEXTURE3);
          gl.bindTexture(gl.TEXTURE_2D, read.textures[1]!);
          gl.bindFramebuffer(gl.FRAMEBUFFER, write.fb);
          gl.viewport(0, 0, write.w, write.h);
          drawArrays(gl.TRIANGLES, 0, 3);
          const swap = read;
          read = write;
          write = swap;
        }
        gpu?.end();
        stateRead = read;
        stateWrite = write;
        gl.bindFramebuffer(gl.FRAMEBUFFER, scene.fb);
        gl.viewport(0, 0, devW, devH);
      }
      forces = stepForces(forces, dt);

      if (homeTex && seedTex && dustCount > 0) {
        gl.enable(gl.BLEND);
        gl.blendFunc(gl.ONE, gl.ONE);
        gl.useProgram(dust);
        noteProgram();
        if (! dustStatics) {
          gl.uniform1i(dustU.tHome, 0);
          gl.uniform1i(dustU.tSeed, 1);
          gl.uniform1i(dustU.tOffset, 2);
          gl.uniform1f(dustU.uDepth, BALL_CONFIG.depth);
          gl.uniform1f(dustU.uShape, SHAPES[BALL_CONFIG.shape] ?? 0);
          gl.uniform1f(dustU.uMotion, MOTIONS[BALL_CONFIG.motion] ?? 0);
          gl.uniform1f(dustU.uFill, BALL_CONFIG.fill);
          dustStatics = true;
        }
        gl.uniform1f(dustU.uStirred, stateRead ? 1 : 0);
        gl.uniform2f(dustU.uCenter, width / 2, height / 2);
        gl.uniform2f(dustU.uViewport, width, height);
        gl.uniform1f(dustU.uRadius, radius);
        gl.uniform1f(dustU.uDpr, dpr);
        gl.uniform1f(dustU.uTwinkle, d.twinkle);
        gl.uniform1f(dustU.uPointScale, Math.max(1, radius / 380) * d.size * 1.3);
        gl.uniform1f(dustU.uReveal, unfold);
        gl.uniform1f(dustU.uLift, d.lift);
        gl.uniform1f(dustU.uEncode, encode);
        gl.uniform3fv(dustU["uTones[0]"] as WebGLUniformLocation, tones);
        gl.uniform3fv(dustU.uRimTone, palette.rim);
        gl.uniform1f(dustU.uDustTime, dustTime);
        gl.uniform2f(dustU.uTurn, turnX, turnY);
        gl.activeTexture(gl.TEXTURE0);
        gl.bindTexture(gl.TEXTURE_2D, homeTex);
        gl.activeTexture(gl.TEXTURE1);
        gl.bindTexture(gl.TEXTURE_2D, seedTex);
        gl.activeTexture(gl.TEXTURE2);
        gl.bindTexture(gl.TEXTURE_2D, stateRead ? stateRead.textures[0]! : blank);
        gl.bindVertexArray(dustVao);
        gpu?.begin("dust");
        drawArrays(gl.POINTS, 0, dustCount);
        gpu?.end();
        gl.bindVertexArray(null);
      }

      if (containment && cArgs) {
        gpu?.begin("contain-front");
        gl.enable(gl.BLEND);
        gl.blendFuncSeparate(gl.SRC_ALPHA, gl.ONE_MINUS_SRC_ALPHA, gl.ONE, gl.ONE_MINUS_SRC_ALPHA);
        containment.draw(1, "metal");
        gl.blendFunc(gl.ONE, gl.ONE);
        containment.draw(1, "energy");
        gpu?.end();
      }

      gl.disable(gl.BLEND);
      gl.bindFramebuffer(gl.FRAMEBUFFER, null);
      gl.viewport(0, 0, devW, devH);
      gl.useProgram(composite);
      noteProgram();
      gl.uniform1i(compositeU.tScene, 6);
      gl.uniform1f(compositeU.uLight, 0);
      gl.uniform1f(compositeU.uDecode, 1 / encode);
      gl.activeTexture(gl.TEXTURE6);
      gl.bindTexture(gl.TEXTURE_2D, scene.textures[0]!);
      gl.bindVertexArray(quadVao);
      gpu?.begin("composite");
      drawArrays(gl.TRIANGLES, 0, 3);
      gpu?.end();
      gl.bindVertexArray(null);
      gl.bindTexture(gl.TEXTURE_2D, null);
      perfEnd("ball");
    };

    let life: GlLife = "live";
    const onLost = (e: Event) => {
      e.preventDefault();
      life = glOnLost(life);
    };
    const onRestored = () => {
      if (life !== "lost") {
        return;
      }
      life = glOnRestored(life);
      skipLose.current = true;
      setEpoch((n) => n + 1);
    };
    canvas.addEventListener("webglcontextlost", onLost, false);
    canvas.addEventListener("webglcontextrestored", onRestored, false);
    const onBrush = (ev: PointerEvent) => {
      if (! brush || width < 2 || height < 2) {
        return;
      }
      const rect = canvas.getBoundingClientRect();
      const nx = ((ev.clientX - rect.left) / Math.max(1, rect.width)) * 2 - 1;
      const ny = -(((ev.clientY - rect.top) / Math.max(1, rect.height)) * 2 - 1);
      const slot = spawnForce(forceId, 1, nx, Math.min(1, Math.hypot(ev.movementX, ev.movementY) / 24));
      slot.ox = nx * 0.7;
      slot.oy = ny * 0.7;
      slot.oz = 0;
      slot.dx = ev.movementX;
      slot.dy = -ev.movementY;
      slot.dz = 0;
      const len = Math.hypot(slot.dx, slot.dy) || 1;
      slot.dx /= len;
      slot.dy /= len;
      slot.radius = 0.22;
      slot.life = 0.16;
      slot.speed = 0.4;
      forces = pushForce(forces, slot);
      forceId += 1;
    };
    if (brush) {
      canvas.style.pointerEvents = "auto";
      canvas.addEventListener("pointermove", onBrush);
    }

    const unsubscribe = subscribeVizClock(frame);

    return () => {
      alive = false;
      unsubscribe();
      canvas.removeEventListener("webglcontextlost", onLost);
      canvas.removeEventListener("webglcontextrestored", onRestored);
      canvas.removeEventListener("pointermove", onBrush);
      gpu?.dispose();
      disposeDust();
      disposeTarget(scene);
      gl.deleteTexture(blank);
      gl.deleteBuffer(quadBuf);
      gl.deleteBuffer(dustBuf);
      gl.deleteVertexArray(quadVao);
      gl.deleteVertexArray(dustVao);
      gl.deleteProgram(field);
      gl.deleteProgram(dust);
      gl.deleteProgram(composite);
      containment?.dispose();
      if (stir) {
        gl.deleteProgram(stir);
      }
      if (! skipLose.current) {
        gl.getExtension("WEBGL_lose_context")?.loseContext();
      }
      skipLose.current = false;
    };
  }, [epoch]);

  if (failed) {
    return <>{fallback}</>;
  }
  return (
    <div className={`nk-face-ball absolute inset-0 z-0 ${brushOn ? "" : "pointer-events-none"}`} aria-hidden>
      <canvas ref={ref} className="block h-full w-full" />
    </div>
  );
}
