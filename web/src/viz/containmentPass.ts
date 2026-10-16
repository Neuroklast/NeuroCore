import {
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

float arcMask(float theta, float rot, float n, float gap, float phase) {
  float a = theta - rot - phase;
  a = a - TAU * floor(a / TAU);
  float u = fract(a / TAU * max(n, 1.0));
  float on = clamp(1.0 - gap, 0.2, 0.95);
  return smoothstep(0.0, 0.035, u) * smoothstep(on + 0.035, on, u);
}

vec3 shadeMetal(vec3 n, float bright) {
  vec3 L = normalize(LIGHT);
  float ndl = clamp(dot(normalize(n), L), 0.0, 1.0);
  vec3 col = mix(uMetalLo, uMetal, 0.4 + 0.6 * ndl);
  col = mix(col, uMetalHi, pow(ndl, 8.0) * 0.72);
  float rim = pow(1.0 - clamp(n.z, 0.0, 1.0), 1.5) * 0.4;
  col += uMetalHi * rim;
  return col * bright;
}

void cover(inout float a, inout vec3 rgb, float d, vec3 n, float bright) {
  float w = 1.0 - smoothstep(-0.9, 1.15, d);
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

  if (! front) {
    for (int i = 0; i < 3; i++) {
      float R = uRingR[i] * (1.0 + uPulse);
      float w = uRingW[i];
      float d = abs(r - R) - w * 0.5;
      float mask = arcMask(theta, uRingRot[i], uRingSeg[i], uRingGap[i], uRingPhase[i]);
      if (mask < 0.01) {
        continue;
      }
      float inner = r - (R - w * 0.5);
      float outer = (R + w * 0.5) - r;
      float bevel = clamp(min(inner, outer) / max(w * 0.42, 0.6), 0.0, 1.0);
      vec2 rad = p / max(r, 1e-3);
      float side = r > R ? 1.0 : -1.0;
      vec3 n = normalize(vec3(rad * side, 0.22 + 0.78 * bevel));
      float bright = uRingBright[i] * (0.82 + 0.18 * mask);
      float segU = fract((theta - uRingRot[i] - uRingPhase[i]) / TAU * uRingSeg[i]);
      float seam = 1.0 - smoothstep(0.0, 0.05, min(segU, max(0.0, 1.0 - uRingGap[i] - segU)));
      bright *= 1.0 - 0.45 * seam * mask;
      float groove = 1.0 - smoothstep(0.0, w * 0.12, abs(r - R));
      bright *= 1.0 - 0.22 * groove;
      float rivetU = fract(theta * 9.0 + float(i) * 0.4);
      float rivet = (1.0 - smoothstep(0.03, 0.07, abs(rivetU - 0.5))) * (1.0 - smoothstep(1.1, 2.0, abs(r - R)));
      bright *= 1.0 - 0.35 * rivet;
      cover(metalA, metalRgb, d + (1.0 - mask) * 6.0, n, bright);
      float lip = 1.0 - smoothstep(0.5, 1.7, abs(r - (R - w * 0.42)));
      if (i == 0) {
        metalRgb = mix(metalRgb, uAccent, lip * mask * 0.22);
        if (energy) {
          glow += uAccent * lip * mask * 0.6;
        }
      }
    }

    float tickR0 = uTicks.x;
    float tickR1 = uTicks.y;
    float nTick = max(uTicks.z, 1.0);
    float tU = fract((theta + PI) / TAU * nTick);
    float tickOn = 1.0 - smoothstep(0.06, 0.12, abs(tU - 0.5));
    float tickD = abs(r - 0.5 * (tickR0 + tickR1)) - 0.5 * (tickR1 - tickR0);
    if (tickOn > 0.05) {
      vec3 n = normalize(vec3(p / max(r, 1e-3), 0.4));
      cover(metalA, metalRgb, tickD + (1.0 - tickOn) * 4.0, n, 0.4);
    }
  }

  for (int k = 0; k < 4; k++) {
    float ang = uClampAng[k];
    float ca = cos(ang);
    float sa = sin(ang);
    vec2 q = vec2(ca * p.x + sa * p.y, -sa * p.x + ca * p.y);
    float innerW = 1.0 - smoothstep(uSplitX - 6.0, uSplitX + 9.0, q.x);
    float outerW = smoothstep(uSplitX - 9.0, uSplitX + 6.0, q.x);
    float zone = front ? innerW : outerW;
    if (zone < 0.01) {
      continue;
    }
    float surge = 1.0 + uSurge * 0.12;
    float d = 1e5;
    vec3 n = vec3(ca, sa, 0.45);
    if (! front) {
      float dYoke = sdBox(q - vec2(uYoke.x, 0.0), vec2(uYoke.y * surge, uYoke.z), 1.7);
      float dHinge = length(q - vec2(uHinge.x, 0.0)) - uHinge.y * surge;
      float dArm = sdBox(q - vec2(uArm.x, 0.0), vec2(uArm.y, uArm.z), 1.05);
      float dBoltA = length(q - vec2(uYoke.x + 1.2, uYoke.z * 0.42)) - 1.55;
      float dBoltB = length(q - vec2(uYoke.x + 1.2, -uYoke.z * 0.42)) - 1.55;
      d = min(min(dYoke, dHinge), dArm);
      n = normalize(vec3(ca, sa, 0.5));
      cover(metalA, metalRgb, d + (1.0 - zone) * 5.0, n, 0.78 + 0.22 * uSurge);
      cover(metalA, metalRgb, dBoltA + (1.0 - zone) * 5.0, vec3(0.1, 0.2, 0.95), 1.05);
      cover(metalA, metalRgb, dBoltB + (1.0 - zone) * 5.0, vec3(0.1, 0.2, 0.95), 1.05);
      float seam = abs(q.y) - 0.6;
      cover(metalA, metalRgb, max(dYoke, seam) + (1.0 - zone) * 5.0, n, 0.45);
    } else {
      float dJaw = sdBox(q - vec2(uJaw.x, 0.0), vec2(uJaw.y * surge, uJaw.z), 2.1);
      float dToothA = sdBox(q - vec2(uJaw.x - uJaw.y * 0.35, uJaw.z * 0.72), vec2(uJaw.y * 0.28, 2.1), 0.7);
      float dToothB = sdBox(q - vec2(uJaw.x - uJaw.y * 0.35, -uJaw.z * 0.72), vec2(uJaw.y * 0.28, 2.1), 0.7);
      float dArm = sdBox(q - vec2(uArm.x * 0.72, 0.0), vec2(uArm.y * 0.5, uArm.z * 1.05), 1.0);
      d = min(min(dJaw, dArm), min(dToothA, dToothB));
      n = normalize(vec3(ca * 0.4, sa * 0.4, 0.82));
      cover(metalA, metalRgb, d + (1.0 - zone) * 5.0, n, 0.9 + 0.25 * uSurge);
      if (energy) {
        float pip = length(q - vec2(uPip.x, 0.0)) - uPip.y * (1.0 + uSurge * 0.4);
        float pipG = 1.0 - smoothstep(-0.4, 1.6, pip);
        glow += uCyan * pipG * (0.55 + 0.45 * uSurge);
        float contact = 1.0 - smoothstep(1.2, 4.5, abs(q.x - pulseR));
        contact *= 1.0 - smoothstep(uJaw.z * 0.85, uJaw.z * 1.4, abs(q.y));
        glow += uAccent * contact * 0.28 * (0.4 + 0.6 * uSurge);
        float dis = uDischarge[k];
        if (dis > 0.01) {
          float arc = abs(q.y) + abs(q.x - pulseR - 3.0) * 0.65;
          float bolt = 1.0 - smoothstep(0.6, 2.8 + dis * 2.0, arc);
          glow += mix(uAccent, uCyan, 0.35) * bolt * dis * 1.4;
        }
      }
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

  if (energy) {
    for (int o = 0; o < 2; o++) {
      vec4 orb = o == 0 ? uOrbit0 : uOrbit1;
      vec2 ab = vec2(orb.x, max(1.0, orb.y * cos(orb.z)));
      vec2 qe = p / ab;
      float dEl = (length(qe) - 1.0) * min(ab.x, ab.y);
      float rail = 1.0 - smoothstep(0.0, 1.35, abs(dEl));
      float hideCore = smoothstep(pulseR * 0.96, pulseR * 1.04, r);
      glow += mix(uAccent, uCyan, 0.55) * rail * hideCore * (front ? 0.18 : 0.28);
    }
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
      float g = 1.0 - smoothstep(-0.4, 2.2, diam);
      float hideCore = smoothstep(pulseR * 0.95, pulseR * 1.02, length(sp));
      glow += mix(uAccent, uCyan, back ? 0.2 : 0.55) * g * hideCore * (back ? 0.45 : 0.8);
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
