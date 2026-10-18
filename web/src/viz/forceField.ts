/** Local travelling audio force fields. Not a global +Y kick. */

export const FORCE_SLOTS = 4;
export const FORCE_AMBIENT = 0.12;

export type ForceBand = 0 | 1 | 2;

export type ForceSlot = {
  id: number;
  ox: number;
  oy: number;
  oz: number;
  dx: number;
  dy: number;
  dz: number;
  radius: number;
  strength: number;
  speed: number;
  age: number;
  life: number;
  band: ForceBand;
};

export const FORCE_EMPTY: ForceSlot = {
  id: 0,
  ox: 0,
  oy: 0,
  oz: 0,
  dx: 0,
  dy: 1,
  dz: 0,
  radius: 0.3,
  strength: 0,
  speed: 0,
  age: 0,
  life: 0,
  band: 0,
};

const BAND = [
  { y: -0.42, r: 0.55, life: 0.28, speed: 0.55, dy: 0.92 },
  { y: -0.04, r: 0.32, life: 0.18, speed: 0.9, dy: 0.4 },
  { y: 0.34, r: 0.18, life: 0.1, speed: 1.25, dy: 0.18 },
] as const;

function hash01(id: number, salt: number): number {
  let v = (id ^ salt) >>> 0;
  v = Math.imul(v ^ (v >>> 16), 0x7feb352d);
  v = Math.imul(v ^ (v >>> 15), 0x846ca68b);
  return ((v ^ (v >>> 16)) >>> 0) / 4294967296;
}

export function forceEnvelope(age: number, life: number): number {
  if (! (life > 0) || age < 0 || age >= life) {
    return 0;
  }
  const u = age / life;
  return (1 - u) * (1 - u);
}

/** Scale so ∫ env dt over the life equals 1. Frame-rate independent impulse energy. */
export function forceGain(slot: ForceSlot): number {
  const env = forceEnvelope(slot.age, slot.life);
  if (! (slot.life > 1e-6) || ! (slot.strength > 0) || env <= 0) {
    return 0;
  }
  return slot.strength * env * (3 / slot.life);
}

export function forceCentre(slot: ForceSlot): [number, number, number] {
  const t = slot.speed * slot.age;
  return [slot.ox + slot.dx * t, slot.oy + slot.dy * t, slot.oz + slot.dz * t];
}

export function forceInfluence(px: number, py: number, pz: number, slot: ForceSlot): number {
  if (! (slot.radius > 1e-6) || ! (slot.strength > 0)) {
    return 0;
  }
  const c = forceCentre(slot);
  const dx = px - c[0];
  const dy = py - c[1];
  const dz = pz - c[2];
  const r2 = slot.radius * slot.radius;
  return Math.exp(-(dx * dx + dy * dy + dz * dz) / (2 * r2));
}

export function spawnForce(
  id: number,
  band: ForceBand,
  balance: number,
  strength: number,
): ForceSlot {
  const t = BAND[band] ?? BAND[0]!;
  const b = Number.isFinite(balance) ? Math.max(-1, Math.min(1, balance)) : 0;
  const s = Number.isFinite(strength) ? Math.max(0, Math.min(1, strength)) : 0;
  const ox = Math.max(-0.72, Math.min(0.72, b * 0.52 + (hash01(id, 3) - 0.5) * 0.1));
  const oy = t.y + (hash01(id, 7) - 0.5) * 0.06;
  const oz = (hash01(id, 11) - 0.5) * 0.18;
  let dx = (hash01(id, 13) - 0.5) * 0.4;
  let dy = t.dy;
  let dz = (hash01(id, 19) - 0.5) * 0.4;
  const len = Math.hypot(dx, dy, dz) || 1;
  dx /= len;
  dy /= len;
  dz /= len;
  return {
    id,
    ox,
    oy,
    oz,
    dx,
    dy,
    dz,
    radius: t.r * (0.92 + 0.16 * hash01(id, 23)),
    strength: s,
    speed: t.speed,
    age: 0,
    life: t.life,
    band,
  };
}

export function emptyForces(): ForceSlot[] {
  return Array.from({ length: FORCE_SLOTS }, () => ({ ...FORCE_EMPTY }));
}

export function pushForce(slots: ForceSlot[], next: ForceSlot): ForceSlot[] {
  const out = slots.map((s) => ({ ...s }));
  let at = out.findIndex((s) => ! (s.strength > 0) || s.age >= s.life);
  if (at < 0) {
    let oldest = 0;
    for (let i = 1; i < out.length; i += 1) {
      if (out[i]!.age > out[oldest]!.age) {
        oldest = i;
      }
    }
    at = oldest;
  }
  out[at] = next;
  return out;
}

export function stepForces(slots: ForceSlot[], dt: number): ForceSlot[] {
  const t = Math.max(0, dt);
  return slots.map((s) => {
    if (! (s.strength > 0) || s.age >= s.life) {
      return { ...FORCE_EMPTY };
    }
    const age = s.age + t;
    if (age >= s.life) {
      return { ...FORCE_EMPTY };
    }
    return { ...s, age };
  });
}

export function packForceUniforms(slots: ForceSlot[]): { pos: Float32Array; kick: Float32Array } {
  const pos = new Float32Array(FORCE_SLOTS * 4);
  const kick = new Float32Array(FORCE_SLOTS * 4);
  for (let i = 0; i < FORCE_SLOTS; i += 1) {
    const s = slots[i] ?? FORCE_EMPTY;
    const c = forceCentre(s);
    const g = forceGain(s);
    pos[i * 4] = c[0];
    pos[i * 4 + 1] = c[1];
    pos[i * 4 + 2] = c[2];
    pos[i * 4 + 3] = s.radius;
    kick[i * 4] = s.dx * g;
    kick[i * 4 + 1] = s.dy * g;
    kick[i * 4 + 2] = s.dz * g;
  }
  return { pos, kick };
}

export function forceAccel(
  px: number,
  py: number,
  pz: number,
  slots: readonly ForceSlot[],
): [number, number, number] {
  let ax = 0;
  let ay = 0;
  let az = 0;
  for (const s of slots) {
    const g = forceGain(s);
    if (! (g > 0)) {
      continue;
    }
    const w = forceInfluence(px, py, pz, s) * g;
    ax += s.dx * w;
    ay += s.dy * w;
    az += s.dz * w;
  }
  return [ax, ay, az];
}

export function bandIndex(name: "low" | "mid" | "high"): ForceBand {
  if (name === "mid") {
    return 1;
  }
  if (name === "high") {
    return 2;
  }
  return 0;
}
