import type { BallDrive } from "./ballModel";

export const STIR_STEP = 1 / 60;
export const STIR_VMAX = 2.2;
export const STIR_SHELL = 0.97;
export const STIR_LIFT_BIAS = 0.1;
export const STIR_SPREAD_BIAS = 0.11;
export const STIR_IMPULSE = 1.55;
export const STIR_DAMP = 1.5;

export type StirP = {
  o: [number, number, number];
  v: [number, number, number];
};

function clampV(v: [number, number, number], max: number): [number, number, number] {
  const s = Math.hypot(v[0], v[1], v[2]);
  if (! (s > max)) {
    return v;
  }
  const g = max / s;
  return [v[0] * g, v[1] * g, v[2] * g];
}

/** One substep. Impulse is a one-shot; continuous lift/spread are rest biases. */
export function stirStep(
  p: StirP,
  home: [number, number, number],
  drive: Pick<BallDrive, "lift" | "spread" | "swirl">,
  impulse: number,
  dt: number,
  k = 1.4,
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
  const bx = nx * drive.spread * STIR_SPREAD_BIAS;
  const by = -drive.lift * STIR_LIFT_BIAS;
  const damp = STIR_DAMP * Math.sqrt(Math.max(k, 1e-6));
  let ax = -k * (ox - bx) - damp * vx;
  let ay = -k * (oy - by) - damp * vy;
  let az = -k * oz - damp * vz;
  const swirl = drive.swirl ?? 0;
  if (swirl > 0) {
    const pace = Math.hypot(vx, vy, vz);
    const spin = Math.max(Math.min(pace, 2), 0.35) * 0.55 * swirl;
    ax += -ny * spin;
    ay += nx * spin;
  }
  if (impulse > 0) {
    const j = impulse * STIR_IMPULSE;
    vx += nx * j * 0.85;
    vy += (ny * 0.35 + 0.28) * j;
    vz += (pz > 1e-4 ? pz / Math.max(Math.hypot(px, py, pz), 1e-4) : 0) * j * 0.4;
  }
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
  impulse: number,
  dt: number,
  k = 1.4,
): StirP {
  let left = Math.max(0, Math.min(0.05, dt));
  let hit = impulse;
  let cur = p;
  while (left > 1e-5) {
    const step = Math.min(STIR_STEP, left);
    cur = stirStep(cur, home, drive, hit, step, k);
    hit = 0;
    left -= step;
  }
  return cur;
}
