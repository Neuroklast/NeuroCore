import { STATE_WIDTH } from "./ballModel";
import { STIR_DAMP, STIR_LIFT_BIAS, STIR_SPREAD_BIAS, STIR_SHELL, STIR_VMAX } from "./stirModel";
import { FORCE_AMBIENT, FORCE_SLOTS } from "./forceField";

// Shared verbatim by the live renderer and browser GPU contracts.
export const HASH = `
uint scramble(uint v) {
  v = v * 747796405u + 2891336453u;
  uint w = ((v >> ((v >> 28u) + 4u)) ^ v) * 277803737u;
  return (w >> 22u) ^ w;
}
float random(uint v) { return float(scramble(v)) * (1.0 / 4294967295.0); }
`;


export const DUST_COMMON = `
uniform float uDustTime;
uniform vec2 uTurn;
uniform float uMotion;
uniform float uFill;

vec3 settle(vec3 p, out float spread) {
  float bend = exp2((0.5 - uFill) * 1.8);
  float h = clamp((p.y + 1.0) * 0.5, 0.0, 1.0);
  float y = 2.0 * pow(h, bend) - 1.0;
  float from = sqrt(max(1.0 - p.y * p.y, 1e-4));
  float to = sqrt(max(1.0 - y * y, 0.0));
  spread = clamp(bend * pow(max(h, 1e-3), bend - 1.0) * to / from, 0.25, 1.0);
  return vec3(p.xz * (to / from), y).xzy;
}

vec3 drift(vec4 home, vec4 seed, uint id, out float life, out float spread) {
  vec3 p = settle(home.xyz, spread);
  float t = uDustTime;
  float u = fract(t / seed.z + seed.y);
  life = sin(3.14159265 * u);
  float travel = seed.w;
  if (uMotion < 0.5) {
    p.y += travel * (u - 0.5);
  } else if (uMotion < 1.5) {
    p.y -= travel * (u - 0.5);
  } else if (uMotion < 2.5) {
    p += 0.045 * vec3(
      sin(t * 0.37 + seed.y * 17.0) + 0.5 * sin(t * 0.83 + seed.z * 5.0),
      sin(t * 0.29 + seed.z * 11.0) + 0.5 * sin(t * 0.61 + seed.w * 7.0),
      sin(t * 0.33 + seed.w * 23.0)
    );
  } else {
    float loop = t * (0.6 + 0.9 * random(id * 7u + 3u)) * (random(id * 7u + 10u) < 0.5 ? -1.0 : 1.0) + seed.y * 6.2831853;
    p.xy += (0.025 + 0.05 * random(id * 7u + 16u)) * vec2(cos(loop), sin(loop));
  }
  float wobble = 0.002 + 0.006 * random(id * 7u + 1u);
  float w1 = 0.8 + 2.4 * random(id * 7u + 2u);
  float w2 = 0.8 + 2.4 * random(id * 7u + 4u);
  p += wobble * vec3(sin(t * w1 + seed.y * 40.0), cos(t * w2 + seed.z * 30.0), sin(t * (w1 + w2) * 0.5 + seed.w * 20.0));
  float cy = cos(uTurn.x);
  float sy = sin(uTurn.x);
  p.xz = mat2(cy, -sy, sy, cy) * p.xz;
  float cx = cos(uTurn.y);
  float sx = sin(uTurn.y);
  p.yz = mat2(cx, -sx, sx, cx) * p.yz;
  return p;
}
`;


export const STIR_FS = `#version 300 es
precision highp float;
uniform sampler2D tHome;
uniform sampler2D tSeed;
uniform sampler2D tOffset;
uniform sampler2D tVelocity;
uniform float uDt;
uniform float uReset;
uniform float uSpread;
uniform float uLiftForce;
uniform float uSwirl;
uniform vec4 uForcePos[${FORCE_SLOTS}];
uniform vec4 uForceKick[${FORCE_SLOTS}];
layout(location = 0) out vec4 outOffset;
layout(location = 1) out vec4 outVelocity;
${HASH}
${DUST_COMMON}
vec3 swirl(vec3 p) {
  float t = uDustTime * 0.15;
  vec3 a = vec3(1.7, 1.9, 1.5) * p.yzx + vec3(t, 1.2, 2.1);
  vec3 b = vec3(2.3, 2.1, 2.7) * p.zxy + vec3(0.4, t * 1.3, 0.9);
  vec3 ca = cos(a) * vec3(1.7, 1.9, 1.5);
  vec3 cb = cos(b) * vec3(2.3, 2.1, 2.7);
  return vec3(ca.z - cb.x, cb.y - ca.x, ca.y - cb.z);
}
void main() {
  ivec2 cell = ivec2(gl_FragCoord.xy);
  if (uReset > 0.5) {
    outOffset = vec4(0.0);
    outVelocity = vec4(0.0);
    return;
  }
  uint id = uint(cell.y * ${STATE_WIDTH} + cell.x);
  vec4 home = texelFetch(tHome, cell, 0);
  vec4 seed = texelFetch(tSeed, cell, 0);
  vec3 o = texelFetch(tOffset, cell, 0).xyz;
  vec3 v = texelFetch(tVelocity, cell, 0).xyz;
  float life;
  float spread;
  vec3 base = drift(home, seed, id, life, spread);
  vec3 p = base + o;

  float k = mix(4.5, 7.5, random(id * 7u + 11u));
  float rxy = length(p.xy);
  vec2 radial = rxy > 1e-4 ? p.xy / rxy : vec2(0.0);
  vec3 bias = vec3(base.x * uSpread * ${STIR_SPREAD_BIAS}, uLiftForce * ${STIR_LIFT_BIAS}, 0.0);
  vec3 acc = -k * (o - bias) - ${STIR_DAMP} * sqrt(k) * v;
  float pace = length(v);
  vec3 heading = pace > 1e-4 ? v / pace : vec3(-radial.y, radial.x, 0.0);
  if (dot(heading, heading) < 1e-8) heading = vec3(1.0, 0.0, 0.0);
  vec3 curl = swirl(p * 1.6);
  acc += curl * ${FORCE_AMBIENT};
  acc += (curl - heading * dot(curl, heading)) * max(min(pace, 2.0), 0.55) * (0.55 * uSwirl);
  for (int i = 0; i < ${FORCE_SLOTS}; i++) {
    vec4 fp = uForcePos[i];
    vec4 fk = uForceKick[i];
    if (fp.w > 1e-6 && dot(fk.xyz, fk.xyz) > 1e-10) {
      vec3 d = p - fp.xyz;
      float inf = exp(-dot(d, d) / (2.0 * fp.w * fp.w));
      acc += fk.xyz * inf;
    }
  }

  v += acc * uDt;
  float speed = length(v);
  if (speed > ${STIR_VMAX}) {
    v *= ${STIR_VMAX} / speed;
  }
  o += v * uDt;
  vec3 q = base + o;
  float len = length(q);
  if (len > ${STIR_SHELL}) {
    vec3 n = q / len;
    o -= n * (len - ${STIR_SHELL});
    v -= n * max(dot(v, n), 0.0) * 1.5;
  }
  outOffset = vec4(o, 1.0);
  outVelocity = vec4(v, 1.0);
}
`;

