import type { BallDrive } from "./ballModel";
import { FORCE_AMBIENT, FORCE_EMPTY, forceAccel, type ForceSlot } from "./forceField";

export const STIR_STEP = 1 / 60;
export const STIR_VMAX = 2.2;
export const STIR_SHELL = 0.97;
export const STIR_LIFT_BIAS = 0.22;
export const STIR_SPREAD_BIAS = 0.16;
export const STIR_DAMP = 1.5;

export type StirP = {
  o: [number, number, number];
  v: [number, number, number];
};

export function rest(): StirP {
  return { o: [0, 0, 0], v: [0, 0, 0] };
}

function clampV(v: [number, number, number], max: number): [number, number, number] {
  const s = Math.hypot(v[0], v[1], v[2]);
  if (! (s > max)) {
    return v;
  }
  const g = max / s;
  return [v[0] * g, v[1] * g, v[2] * g];
}

function swirlAt(p: [number, number, number], t: number): [number, number, number] {
  const a0 = 1.7 * p[1] + t;
  const a1 = 1.9 * p[2] + 1.2;
  const a2 = 1.5 * p[0] + 2.1;
  const b0 = 2.3 * p[2] + 0.4;
  const b1 = 2.1 * p[0] + t * 1.3;
  const b2 = 2.7 * p[1] + 0.9;
  return [
    Math.cos(a2) * 1.5 - Math.cos(b0) * 2.3,
    Math.cos(b1) * 2.1 - Math.cos(a0) * 1.7,
    Math.cos(a1) * 1.9 - Math.cos(b2) * 2.7,
  ];
}

/** One substep. Audio events are local fields; impulse energy is not a global +Y. */
export function stirStep(
  p: StirP,
  home: [number, number, number],
  drive: Pick<BallDrive, "lift" | "spread" | "swirl">,
  forces: readonly ForceSlot[],
  dt: number,
  k = 6,
  time = 0,
): StirP {
  if (! (dt > 0) || ! Number.isFinite(dt)) {
    return p;
  }
  const hx = home[0];
  const hy = home[1];
  const hz = home[2];
  let ox = p.o[0];
  let oy = p.o[1];
  let oz = p.o[2];
  let vx = p.v[0];
  let vy = p.v[1];
  let vz = p.v[2];
  const px = hx + ox;
  const py = hy + oy;
  const pz = hz + oz;
  const rxy = Math.hypot(px, py);
  const nx = rxy > 1e-4 ? px / rxy : 0;
  const ny = rxy > 1e-4 ? py / rxy : 0;
  const bx = hx * drive.spread * STIR_SPREAD_BIAS;
  const by = drive.lift * STIR_LIFT_BIAS;
  const damp = STIR_DAMP * Math.sqrt(Math.max(k, 1e-6));
  let ax = -k * (ox - bx) - damp * vx;
  let ay = -k * (oy - by) - damp * vy;
  let az = -k * oz - damp * vz;
  const curl = swirlAt([px * 1.6, py * 1.6, pz * 1.6], time * 0.15);
  ax += curl[0] * FORCE_AMBIENT;
  ay += curl[1] * FORCE_AMBIENT;
  az += curl[2] * FORCE_AMBIENT;
  const swirl = drive.swirl ?? 0;
  if (swirl > 0) {
    const pace = Math.hypot(vx, vy, vz);
    const spin = Math.max(Math.min(pace, 2), 0.55) * 0.55 * swirl;
    ax += -ny * spin;
    ay += nx * spin;
  }
  const audio = forceAccel(px, py, pz, forces);
  ax += audio[0];
  ay += audio[1];
  az += audio[2];
  vx += ax * dt;
  vy += ay * dt;
  vz += az * dt;
  [vx, vy, vz] = clampV([vx, vy, vz], STIR_VMAX);
  ox += vx * dt;
  oy += vy * dt;
  oz += vz * dt;
  const qx = hx + ox;
  const qy = hy + oy;
  const qz = hz + oz;
  const len = Math.hypot(qx, qy, qz);
  if (len > STIR_SHELL) {
    const inv = 1 / len;
    const nnx = qx * inv;
    const nny = qy * inv;
    const nnz = qz * inv;
    const extra = len - STIR_SHELL;
    ox -= nnx * extra;
    oy -= nny * extra;
    oz -= nnz * extra;
    const push = Math.max(vx * nnx + vy * nny + vz * nnz, 0) * 1.5;
    vx -= nnx * push;
    vy -= nny * push;
    vz -= nnz * push;
  }
  return { o: [ox, oy, oz], v: [vx, vy, vz] };
}

export function stirIntegrate(
  p: StirP,
  home: [number, number, number],
  drive: Pick<BallDrive, "lift" | "spread" | "swirl">,
  forces: readonly ForceSlot[] = [FORCE_EMPTY],
  dt: number,
  k = 6,
  time = 0,
): StirP {
  let left = Math.max(0, Math.min(0.05, dt));
  let cur = p;
  let t = time;
  while (left > 1e-5) {
    const step = Math.min(STIR_STEP, left);
    cur = stirStep(cur, home, drive, forces, step, k, t);
    t += step;
    left -= step;
  }
  return cur;
}
