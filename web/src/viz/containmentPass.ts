import {
  BRACE_ANGLES,
  CLAMP_ANGLES,
  SHARD_COUNT,
  type ContainmentLayout,
  type ContainmentPalette,
  type ContainmentPose,
  type OrbitShard,
} from "./containmentModel";

const PASS_VS = `#version 300 es
layout(location = 0) in vec2 position;
out vec2 vUv;
void main() {
  vUv = position * 0.5 + 0.5;
  gl_Position = vec4(position, 0.0, 1.0);
}
`;

const CONTAINMENT_FS = `#version 300 es
precision highp float;
uniform vec2 uCenter;
uniform float uBallR;
uniform float uDpr;
uniform float uEncode;
uniform float uLayer;
uniform float uPass;
uniform float uPulse;
uniform float uSurge;
uniform float uGlow;
uniform vec3 uMetalLo;
uniform vec3 uMetal;
uniform vec3 uMetalHi;
uniform vec3 uAccent;
uniform vec3 uCyan;
uniform vec3 uRingR;
uniform vec3 uRingW;
uniform vec3 uRingRot;
uniform vec3 uRingGap;
uniform vec3 uRingSeg;
uniform vec3 uRingPhase;
uniform vec3 uRingBright;
uniform vec4 uJaw;
uniform vec4 uArm;
uniform vec4 uYoke;
uniform vec3 uHinge;
uniform vec3 uPip;
uniform float uSplitX;
uniform float uClampAng[4];
uniform float uBraceAng[4];
uniform float uDischarge[4];
uniform vec4 uOrbit0;
uniform vec4 uOrbit1;
uniform vec4 uTicks;
uniform vec2 uBracket;
uniform vec4 uShards[16];
in vec2 vUv;
out vec4 fragColor;

const float PI = 3.14159265;
const float TAU = 6.28318531;
const vec3 LIGHT = vec3(-0.36, 0.7, 0.62);

float sdBox(vec2 p, vec2 b, float r) {
  vec2 q = abs(p) - b + r;
  return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}

float sdCapsule(vec2 p, vec2 a, vec2 b, float r) {
  vec2 pa = p - a;
  vec2 ba = b - a;
  float h = clamp(dot(pa, ba) / max(dot(ba, ba), 1e-5), 0.0, 1.0);
  return length(pa - ba * h) - r;
}

float arcMask(float theta, float rot, float n, float gap, float phase) {
  float a = theta - rot - phase;
  a = a - TAU * floor(a / TAU);
  float u = fract(a / TAU * max(n, 1.0));
  float on = clamp(1.0 - gap, 0.2, 0.95);
  float aa = 0.008;
  return smoothstep(0.0, aa, u) * smoothstep(on + aa, on, u);
}

vec3 shadeMetal(vec3 n, float bright) {
  vec3 nn = normalize(n);
  vec3 L = normalize(LIGHT);
  float ndl = clamp(dot(nn, L), 0.0, 1.0);
  vec3 col = mix(uMetalLo, uMetal, 0.25 + 0.75 * ndl);
  vec3 H = normalize(L + vec3(0.0, 0.0, 1.0));
  col = mix(col, uMetalHi, pow(clamp(dot(nn, H), 0.0, 1.0), 28.0) * 0.95);
  col += uMetalHi * pow(1.0 - clamp(nn.z, 0.0, 1.0), 2.4) * 0.35;
  return col * bright;
}

void cover(inout float a, inout vec3 rgb, float d, vec3 n, float bright) {
  float w = 1.0 - smoothstep(-0.4, 0.5, d);
  if (w <= 0.001) {
    return;
  }
  vec3 c = shadeMetal(n, bright);
  rgb = mix(rgb, c, w * (1.0 - a * 0.35));
  a = a + w * (1.0 - a);
}

void main() {
  vec2 p = gl_FragCoord.xy / uDpr - uCenter;
  float r = length(p);
  float theta = atan(p.y, p.x);
  bool front = uLayer > 0.5;
  bool energy = uPass > 0.5;
  float pulseR = uBallR * (1.0 + uPulse);
  if (! front && r < pulseR * 0.985) {
    discard;
  }

  float metalA = 0.0;
  vec3 metalRgb = vec3(0.0);
  vec3 glow = vec3(0.0);

  for (int i = 0; i < 3; i++) {
    float R = uRingR[i] * (1.0 + uPulse);
    float w = uRingW[i];
    float mask = arcMask(theta, uRingRot[i], uRingSeg[i], uRingGap[i], uRingPhase[i]);
    if (mask < 0.01) {
      continue;
    }
    float dBody = abs(r - R) - w * 0.5;
    float dRail = abs(r - (R - w * 0.32)) - w * 0.11;
    float dLip = abs(r - (R + w * 0.28)) - w * 0.07;
    float dCond = abs(r - (R - w * 0.18)) - w * 0.045;
    float inner = r - (R - w * 0.5);
    float outer = (R + w * 0.5) - r;
    float bevel = clamp(min(inner, outer) / max(w * 0.38, 0.7), 0.0, 1.0);
    vec2 rad = p / max(r, 1e-3);
    vec3 n = normalize(vec3(rad * (r > R ? 1.0 : -1.0), 0.12 + 0.88 * bevel));
    float segU = fract((theta - uRingRot[i] - uRingPhase[i]) / TAU * uRingSeg[i]);
    float seam = 1.0 - smoothstep(0.0, 0.04, min(segU, max(0.0, 1.0 - uRingGap[i] - segU)));
    float bright = uRingBright[i] * (0.72 + 0.28 * mask) * (1.0 - 0.55 * seam);
    float groove = 1.0 - smoothstep(0.0, w * 0.1, abs(r - R));
    bright *= 1.0 - 0.22 * groove;
    float rivetU = fract(theta * (10.0 + float(i) * 3.0) + uRingPhase[i]);
    float rivet = (1.0 - smoothstep(0.028, 0.07, abs(rivetU - 0.5))) * (1.0 - smoothstep(w * 0.2, w * 0.55, abs(r - R)));
    if (! front) {
      float z = (1.0 - mask) * 6.0;
      cover(metalA, metalRgb, dBody + z, n, bright);
      cover(metalA, metalRgb, dRail + z, n, bright * 1.18);
      cover(metalA, metalRgb, dLip + z, n, bright * 0.82);
      float joint = abs(r - R) - w * 0.72;
      cover(metalA, metalRgb, joint + (1.0 - seam) * 8.0, vec3(rad, 0.55), 1.08);
      cover(metalA, metalRgb, length(vec2(abs(r - R), (rivetU - 0.5) * R * 0.12)) - 1.15 + (1.0 - rivet) * 6.0, vec3(0.1, 0.15, 1.0), 1.2);
      metalRgb = mix(metalRgb, uAccent, (1.0 - smoothstep(-0.35, 0.85, dCond)) * mask * 0.55);
      if (energy) {
        glow += uAccent * mask * (0.18 + 0.5 * uGlow) * (1.0 - smoothstep(0.3, 1.6, abs(dCond)));
      }
    } else if (i == 0 && r > pulseR * 0.94) {
      float lip = abs(r - (R - w * 0.42)) - w * 0.08;
      cover(metalA, metalRgb, lip + (1.0 - mask) * 6.0, n, 1.05);
      metalRgb = mix(metalRgb, uAccent, (1.0 - smoothstep(0.4, 1.6, abs(r - (R - w * 0.42)))) * mask * 0.45);
      if (energy) {
        glow += uAccent * mask * uGlow * 0.55 * (1.0 - smoothstep(0.4, 1.8, abs(r - (R - w * 0.42))));
      }
    }
  }

  if (front) {
    float tickR0 = uTicks.x;
    float tickR1 = uTicks.y;
    float nTick = max(uTicks.z, 1.0);
    float tU = fract((theta + PI) / TAU * nTick);
    float tickOn = 1.0 - smoothstep(0.05, 0.11, abs(tU - 0.5));
    float tickD = abs(r - 0.5 * (tickR0 + tickR1)) - 0.5 * (tickR1 - tickR0);
    if (tickOn > 0.05) {
      cover(metalA, metalRgb, tickD + (1.0 - tickOn) * 4.0, normalize(vec3(p / max(r, 1e-3), 0.45)), 0.48);
    }
  }

  for (int k = 0; k < 4; k++) {
    float ang = uClampAng[k];
    float ca = cos(ang);
    float sa = sin(ang);
    vec2 q = vec2(ca * p.x + sa * p.y, -sa * p.x + ca * p.y);
    float innerW = 1.0 - smoothstep(uSplitX - 8.0, uSplitX + 12.0, q.x);
    float outerW = smoothstep(uSplitX - 12.0, uSplitX + 8.0, q.x);
    float zone = front ? innerW : outerW;
    if (zone < 0.01) {
      continue;
    }
    float surge = 1.0 + uSurge * 0.14;
    vec3 n = normalize(vec3(ca, sa, 0.55));
    float z = 1.0 - zone;
    float top = smoothstep(uJaw.z * 0.15, -uJaw.z * 0.85, q.y);
    vec3 nLit = normalize(vec3(ca * 0.45, sa * 0.45, 0.35 + 0.65 * top));
    if (! front) {
      float dHouse = sdBox(q - vec2(uYoke.x, 0.0), vec2(uYoke.y * surge, uYoke.z), 3.2);
      float dCut = sdBox(q - vec2(uYoke.x + uYoke.y * 0.18, 0.0), vec2(uYoke.y * 0.5, uYoke.z * 0.42), 1.2);
      float dCollar = sdBox(q - vec2(uHinge.x, 0.0), vec2(uArm.y * 0.62, uArm.z * 2.35), 1.6);
      float dHinge = length(q - vec2(uHinge.x, 0.0)) - uHinge.y * surge * 1.15;
      float dHingeHole = length(q - vec2(uHinge.x, 0.0)) - uHinge.y * 0.42;
      float dArm = sdCapsule(q, vec2(uArm.x - uArm.y, 0.0), vec2(uYoke.x - uYoke.y * 0.2, 0.0), uArm.z * 1.35);
      float dFin = sdBox(q - vec2(uYoke.x - uYoke.y * 0.2, 0.0), vec2(1.1, uYoke.z * 0.92), 0.3);
      float dBoltA = length(q - vec2(uYoke.x + 1.8, uYoke.z * 0.52)) - 2.15;
      float dBoltB = length(q - vec2(uYoke.x + 1.8, -uYoke.z * 0.52)) - 2.15;
      float dBoltC = length(q - vec2(uHinge.x - 1.2, uHinge.y * 0.7)) - 1.35;
      cover(metalA, metalRgb, dHouse + z * 5.0, nLit, 0.7 + 0.22 * uSurge);
      cover(metalA, metalRgb, max(dHouse, -dCut - 0.8) + z * 5.0, vec3(ca, sa, 0.18), 0.38);
      cover(metalA, metalRgb, dCollar + z * 5.0, nLit, 0.88);
      cover(metalA, metalRgb, max(dHinge, -dHingeHole) + z * 5.0, vec3(0.12, 0.18, 0.98), 1.12);
      cover(metalA, metalRgb, dArm + z * 5.0, nLit, 0.8);
      cover(metalA, metalRgb, dFin + z * 5.0, n, 0.62);
      cover(metalA, metalRgb, dBoltA + z * 5.0, vec3(0.08, 0.12, 1.0), 1.18);
      cover(metalA, metalRgb, dBoltB + z * 5.0, vec3(0.08, 0.12, 1.0), 1.18);
      cover(metalA, metalRgb, dBoltC + z * 5.0, vec3(0.1, 0.14, 1.0), 1.1);
    } else {
      float radY = uJaw.z * 0.62;
      float dShroud = sdCapsule(q, vec2(uJaw.x - uJaw.y * 0.55, 0.0), vec2(uJaw.x + uJaw.y * 0.7, 0.0), radY * 1.08);
      float dBarrel = sdCapsule(q, vec2(uJaw.x - uJaw.y * 0.35, 0.0), vec2(uJaw.x + uJaw.y * 0.55, 0.0), radY);
      float dHead = sdBox(q - vec2(uPip.x + 2.0, 0.0), vec2(uJaw.y * 0.38, uJaw.z * 0.92), radY * 0.45);
      float dWell = length(q - vec2(uPip.x, 0.0)) - uJaw.z * 0.34;
      float dFlangeA = abs(length(q - vec2(uJaw.x - uJaw.y * 0.15, 0.0)) - radY * 1.02) - 1.35;
      float dFlangeB = abs(length(q - vec2(uJaw.x + uJaw.y * 0.35, 0.0)) - radY * 0.95) - 1.15;
      float dGroove = abs(q.y) - 0.55;
      vec3 nf = normalize(vec3(ca * 0.25, sa * 0.25, 0.55 + 0.45 * top));
      cover(metalA, metalRgb, dShroud + z * 5.0, nf, 0.78 + 0.18 * uSurge);
      cover(metalA, metalRgb, dBarrel + z * 5.0, nf, 1.02 + 0.18 * uSurge);
      cover(metalA, metalRgb, dHead + z * 5.0, nf, 1.08);
      cover(metalA, metalRgb, max(dBarrel, dGroove) + z * 5.0, vec3(ca, sa, 0.2), 0.42);
      cover(metalA, metalRgb, dFlangeA + z * 5.0, nLit, 1.12);
      cover(metalA, metalRgb, dFlangeB + z * 5.0, nLit, 1.05);
      cover(metalA, metalRgb, dWell + z * 5.0, vec3(0.0, 0.0, 0.15), 0.28);
      if (energy) {
        float pip = length(q - vec2(uPip.x, 0.0)) - uPip.y * (1.35 + uSurge * 0.5);
        glow += uCyan * (1.0 - smoothstep(-0.6, 2.1, pip)) * (0.4 + 0.6 * uGlow);
        float bezel = abs(length(q - vec2(uPip.x, 0.0)) - uPip.y * 2.1) - 0.7;
        glow += uCyan * (1.0 - smoothstep(0.0, 1.2, bezel)) * 0.22;
        float contact = (1.0 - smoothstep(1.0, 5.5, abs(q.x - pulseR)))
          * (1.0 - smoothstep(uJaw.z * 0.7, uJaw.z * 1.35, abs(q.y)));
        glow += uAccent * contact * (0.35 + 0.65 * uSurge) * (0.5 + 0.5 * uGlow);
        float dis = uDischarge[k];
        if (dis > 0.01) {
          float arc = abs(q.y) + abs(q.x - pulseR - 2.5) * 0.55;
          glow += mix(uAccent, uCyan, 0.3) * (1.0 - smoothstep(0.5, 2.6 + dis * 2.2, arc)) * dis * 1.6;
        }
      }
    }
    if (! energy) {
      vec2 emit = vec2(ca, sa) * (pulseR + 4.0);
      float att = exp(-length(p - emit) * 0.045) * (0.2 + 0.8 * uSurge);
      metalRgb += uAccent * att * 0.22 * metalA;
    }
  }

  for (int b = 0; b < 4; b++) {
    float ang = uBraceAng[b];
    float ca = cos(ang);
    float sa = sin(ang);
    vec2 q = vec2(ca * p.x + sa * p.y, -sa * p.x + ca * p.y);
    float sc = 0.38;
    if (front) {
      float dPad = sdBox(q - vec2(uJaw.x, 0.0), vec2(uJaw.y * sc, uJaw.z * sc * 0.7), 0.8);
      cover(metalA, metalRgb, dPad, normalize(vec3(ca * 0.4, sa * 0.4, 0.85)), 0.7);
    } else {
      float dBar = sdBox(q - vec2(uArm.x, 0.0), vec2(uArm.y * sc * 1.1, uArm.z * sc * 1.4), 0.7);
      float dCap = sdBox(q - vec2(uYoke.x * 0.92, 0.0), vec2(uYoke.y * sc * 0.7, uYoke.z * sc * 0.55), 1.0);
      cover(metalA, metalRgb, dBar, normalize(vec3(ca, sa, 0.5)), 0.55);
      cover(metalA, metalRgb, dCap, normalize(vec3(ca, sa, 0.45)), 0.6);
    }
  }

  if (front) {
    float span = uBracket.x;
    float arm = uBracket.y;
    vec2 ap = abs(p) - vec2(span, span);
    float h = sdBox(ap - vec2(arm * 0.5, 0.0), vec2(arm * 0.5, 0.7), 0.0);
    float v = sdBox(ap - vec2(0.0, arm * 0.5), vec2(0.7, arm * 0.5), 0.0);
    float br = min(h, v);
    if (energy) {
      glow += uCyan * (1.0 - smoothstep(0.0, 1.2, br)) * 0.22;
    } else {
      vec3 n = vec3(0.0, 0.0, 1.0);
      cover(metalA, metalRgb, br, n, 0.55);
    }
  }

  for (int o = 0; o < 2; o++) {
    vec4 orb = o == 0 ? uOrbit0 : uOrbit1;
    vec2 ab = vec2(orb.x, max(1.0, orb.y * cos(orb.z)));
    vec2 qe = p / ab;
    float dEl = (length(qe) - 1.0) * min(ab.x, ab.y);
    float hideCore = smoothstep(pulseR * 1.04, pulseR * 1.16, r);
    bool backHalf = (p.y * orb.z) > 0.0;
    if (backHalf == front) {
      continue;
    }
    float dock = 1.0 - smoothstep(4.0, 14.0, abs(r - uRingR[2]));
    if (! energy) {
      cover(metalA, metalRgb, abs(dEl) - 0.85, normalize(vec3(p / max(r, 1e-3), 0.4)), (front ? 0.5 : 0.22) * hideCore);
      cover(metalA, metalRgb, abs(dEl) - 1.6 + (1.0 - dock) * 8.0, vec3(0.2, 0.15, 0.9), 0.7 * hideCore);
    } else {
      float rail = 1.0 - smoothstep(0.0, 1.45, abs(dEl));
      float dim = front ? 0.62 * (0.45 + 0.55 * uGlow) : 0.22 * (0.35 + 0.4 * uGlow);
      glow += mix(uAccent, uCyan, front ? 0.35 : 0.1) * rail * hideCore * dim;
      glow += uAccent * dock * rail * hideCore * 0.25;
    }
  }
  if (energy) {
    for (int s = 0; s < 16; s++) {
      vec4 sh = uShards[s];
      if (sh.w < 0.2) {
        continue;
      }
      bool back = sh.z < 0.0;
      if (back == front) {
        continue;
      }
      vec2 sp = vec2(sh.x - uCenter.x, uCenter.y - sh.y);
      vec2 d = p - sp;
      float diam = (abs(d.x) + abs(d.y)) - sh.w;
      float g = 1.0 - smoothstep(-0.3, 1.8, diam);
      float hideCore = smoothstep(pulseR * 1.0, pulseR * 1.08, length(sp));
      glow += mix(uAccent, uCyan, back ? 0.12 : 0.45) * g * hideCore * (back ? 0.4 : 1.05) * (0.45 + 0.55 * uGlow);
    }
  }

  if (energy) {
    if (dot(glow, glow) < 1e-6) {
      discard;
    }
    fragColor = vec4(glow * uEncode, 0.0);
    return;
  }
  if (metalA < 0.02) {
    discard;
  }
  fragColor = vec4(metalRgb * uEncode, metalA);
}
`;

function compile(gl: WebGL2RenderingContext, type: number, src: string): WebGLShader | null {
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
}

function linkOrNull(gl: WebGL2RenderingContext, vsSrc: string, fsSrc: string): WebGLProgram | null {
  const vs = compile(gl, gl.VERTEX_SHADER, vsSrc);
  const fs = compile(gl, gl.FRAGMENT_SHADER, fsSrc);
  if (! vs || ! fs) {
    if (vs) {
      gl.deleteShader(vs);
    }
    if (fs) {
      gl.deleteShader(fs);
    }
    return null;
  }
  const prog = gl.createProgram();
  if (! prog) {
    gl.deleteShader(vs);
    gl.deleteShader(fs);
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

export type ContainmentDraw = {
  layout: ContainmentLayout;
  pose: ContainmentPose;
  shards: OrbitShard[];
  palette: ContainmentPalette;
  dpr: number;
  encode: number;
};

export type ContainmentGpu = {
  draw: (layer: 0 | 1, pass: "metal" | "energy", args: ContainmentDraw) => void;
  dispose: () => void;
};

export function createContainment(gl: WebGL2RenderingContext): ContainmentGpu | null {
  const prog = linkOrNull(gl, PASS_VS, CONTAINMENT_FS);
  if (! prog) {
    return null;
  }
  const loc = (name: string) => gl.getUniformLocation(prog, name);
  const u = {
    uCenter: loc("uCenter"),
    uBallR: loc("uBallR"),
    uDpr: loc("uDpr"),
    uEncode: loc("uEncode"),
    uLayer: loc("uLayer"),
    uPass: loc("uPass"),
    uPulse: loc("uPulse"),
    uSurge: loc("uSurge"),
    uGlow: loc("uGlow"),
    uMetalLo: loc("uMetalLo"),
    uMetal: loc("uMetal"),
    uMetalHi: loc("uMetalHi"),
    uAccent: loc("uAccent"),
    uCyan: loc("uCyan"),
    uRingR: loc("uRingR"),
    uRingW: loc("uRingW"),
    uRingRot: loc("uRingRot"),
    uRingGap: loc("uRingGap"),
    uRingSeg: loc("uRingSeg"),
    uRingPhase: loc("uRingPhase"),
    uRingBright: loc("uRingBright"),
    uJaw: loc("uJaw"),
    uArm: loc("uArm"),
    uYoke: loc("uYoke"),
    uHinge: loc("uHinge"),
    uPip: loc("uPip"),
    uSplitX: loc("uSplitX"),
    uClampAng: loc("uClampAng[0]"),
    uBraceAng: loc("uBraceAng[0]"),
    uDischarge: loc("uDischarge[0]"),
    uOrbit0: loc("uOrbit0"),
    uOrbit1: loc("uOrbit1"),
    uTicks: loc("uTicks"),
    uBracket: loc("uBracket"),
    uShards: loc("uShards[0]"),
  };
  const vao = gl.createVertexArray();
  const buf = gl.createBuffer();
  gl.bindVertexArray(vao);
  gl.bindBuffer(gl.ARRAY_BUFFER, buf);
  gl.bufferData(gl.ARRAY_BUFFER, new Float32Array([-1, -1, 3, -1, -1, 3]), gl.STATIC_DRAW);
  gl.enableVertexAttribArray(0);
  gl.vertexAttribPointer(0, 2, gl.FLOAT, false, 0, 0);
  gl.bindVertexArray(null);

  const shardPack = new Float32Array(SHARD_COUNT * 4);
  const clampAng = new Float32Array(CLAMP_ANGLES);
  const braceAng = new Float32Array(BRACE_ANGLES);
  const discharge = new Float32Array(4);

  return {
    draw(layer, pass, args) {
      const { layout: L, pose, shards, palette, dpr, encode } = args;
      gl.useProgram(prog);
      gl.bindVertexArray(vao);
      gl.uniform2f(u.uCenter, L.cx, L.cy);
      gl.uniform1f(u.uBallR, L.ballR);
      gl.uniform1f(u.uDpr, dpr);
      gl.uniform1f(u.uEncode, encode);
      gl.uniform1f(u.uLayer, layer);
      gl.uniform1f(u.uPass, pass === "energy" ? 1 : 0);
      gl.uniform1f(u.uPulse, pose.pulse);
      gl.uniform1f(u.uSurge, pose.clampSurge);
      gl.uniform1f(u.uGlow, pose.glow);
      gl.uniform3fv(u.uMetalLo, palette.metalLo);
      gl.uniform3fv(u.uMetal, palette.metal);
      gl.uniform3fv(u.uMetalHi, palette.metalHi);
      gl.uniform3fv(u.uAccent, palette.accent);
      gl.uniform3fv(u.uCyan, palette.cyan);
      const rings = L.rings;
      gl.uniform3f(u.uRingR, rings[0]!.r, rings[1]!.r, rings[2]!.r);
      gl.uniform3f(u.uRingW, rings[0]!.thick, rings[1]!.thick, rings[2]!.thick);
      gl.uniform3f(u.uRingRot, pose.ringRot[0], pose.ringRot[1], pose.ringRot[2]);
      gl.uniform3f(u.uRingGap, rings[0]!.gap, rings[1]!.gap, rings[2]!.gap);
      gl.uniform3f(u.uRingSeg, rings[0]!.segments, rings[1]!.segments, rings[2]!.segments);
      gl.uniform3f(u.uRingPhase, rings[0]!.phase, rings[1]!.phase, rings[2]!.phase);
      gl.uniform3f(u.uRingBright, rings[0]!.bright, rings[1]!.bright, rings[2]!.bright);
      const c = L.clamp;
      gl.uniform4f(u.uJaw, c.jawX, c.jawHx, c.jawHy, 0);
      gl.uniform4f(u.uArm, c.armX, c.armHx, c.armHy, 0);
      gl.uniform4f(u.uYoke, c.yokeX, c.yokeHx, c.yokeHy, 0);
      gl.uniform3f(u.uHinge, c.hingeX, c.hingeR, 0);
      gl.uniform3f(u.uPip, c.pipX, c.pipR, 0);
      gl.uniform1f(u.uSplitX, c.splitX);
      gl.uniform1fv(u.uClampAng, clampAng);
      gl.uniform1fv(u.uBraceAng, braceAng);
      discharge[0] = pose.discharge[0];
      discharge[1] = pose.discharge[1];
      discharge[2] = pose.discharge[2];
      discharge[3] = pose.discharge[3];
      gl.uniform1fv(u.uDischarge, discharge);
      const o0 = L.orbits[0]!;
      const o1 = L.orbits[1]!;
      gl.uniform4f(u.uOrbit0, o0.rx, o0.ry, o0.tilt, 0);
      gl.uniform4f(u.uOrbit1, o1.rx, o1.ry, o1.tilt, 0);
      gl.uniform4f(u.uTicks, L.ticks.inner, L.ticks.outer, L.ticks.count, 0);
      gl.uniform2f(u.uBracket, L.brackets.span, L.brackets.arm);
      shardPack.fill(0);
      for (let i = 0; i < shards.length && i < SHARD_COUNT; i += 1) {
        const s = shards[i]!;
        shardPack[i * 4] = s.x;
        shardPack[i * 4 + 1] = s.y;
        shardPack[i * 4 + 2] = s.z;
        shardPack[i * 4 + 3] = s.size;
      }
      gl.uniform4fv(u.uShards, shardPack);
      gl.drawArrays(gl.TRIANGLES, 0, 3);
      gl.bindVertexArray(null);
    },
    dispose() {
      gl.deleteBuffer(buf);
      gl.deleteVertexArray(vao);
      gl.deleteProgram(prog);
    },
  };
}
