import { describe, expect, it } from "vitest";
import { buildDust } from "./ballModel";
import { emptyForces, spawnForce, stepForces, type ForceSlot } from "./forceField";
import { rest, stirIntegrate, type StirP } from "./stirModel";

function cloud(n: number): { home: [number, number, number]; p: StirP }[] {
  const dust = buildDust(n);
  const out: { home: [number, number, number]; p: StirP }[] = [];
  for (let i = 0; i < Math.min(n, dust.count); i += 1) {
    out.push({
      home: [dust.home[i * 4]!, dust.home[i * 4 + 1]!, dust.home[i * 4 + 2]!],
      p: rest(),
    });
  }
  return out;
}

function meanY(c: { home: [number, number, number]; p: StirP }[]): number {
  let s = 0;
  for (const x of c) {
    s += x.home[1] + x.p.o[1];
  }
  return s / Math.max(1, c.length);
}

function meanR(c: { home: [number, number, number]; p: StirP }[]): number {
  let s = 0;
  for (const x of c) {
    s += Math.hypot(x.home[0] + x.p.o[0], x.home[1] + x.p.o[1], x.home[2] + x.p.o[2]);
  }
  return s / Math.max(1, c.length);
}

function run(
  seconds: number,
  dt: number,
  drive: { lift: number; spread: number; swirl?: number },
  forces: () => ForceSlot[] = emptyForces,
): { y: number; r: number; vmax: number } {
  const c = cloud(32);
  const y0 = meanY(c);
  let t = 0;
  let vmax = 0;
  let n = 0;
  while (t < seconds - 1e-9) {
    const field = forces();
    for (const x of c) {
      x.p = stirIntegrate(x.p, x.home, drive, field, dt, 6, t);
      vmax = Math.max(vmax, Math.hypot(x.p.v[0], x.p.v[1], x.p.v[2]));
    }
    t += dt;
    n += 1;
  }
  expect(n).toBeGreaterThan(10);
  return { y: meanY(c) - y0, r: meanR(c), vmax };
}

describe("stir rest bias", () => {
  it("60s silence keeps the centroid and stays finite", () => {
    const hit = run(60, 1 / 60, { lift: 0, spread: 0 });
    expect(Math.abs(hit.y)).toBeLessThan(0.08);
    expect(hit.vmax).toBeLessThan(STIR_VMAX_CHECK);
    expect(Number.isFinite(hit.vmax)).toBe(true);
  });

  it("60s bass pulses but does not pile at the top or the shell", () => {
    const hit = run(60, 1 / 60, { lift: 1, spread: 0.4 });
    expect(hit.y).toBeLessThan(0.3);
    expect(hit.y).toBeGreaterThan(0.08);
    expect(hit.r).toBeLessThan(0.95);
  });

  it("swirl from rest starts motion without a +Y lift force", () => {
    const hit = run(1, 1 / 60, { lift: 0, spread: 0, swirl: 1 });
    expect(hit.vmax).toBeGreaterThan(0.05);
  });

  it("a local field kicks then returns", () => {
    const home: [number, number, number] = [0, -0.55, 0];
    const field = { ...spawnForce(4, 0, 0, 1), ox: 0, oy: -0.55, oz: 0, dx: 0, dy: 1, dz: 0, radius: 0.4, speed: 0 };
    let p = rest();
    p = stirIntegrate(p, home, { lift: 0, spread: 0 }, [field], 1 / 60);
    const kicked = Math.hypot(p.v[0], p.v[1], p.v[2]);
    expect(kicked).toBeGreaterThan(0.15);
    for (let i = 0; i < 90; i += 1) {
      p = stirIntegrate(p, home, { lift: 0, spread: 0 }, emptyForces(), 1 / 60);
    }
    expect(Math.hypot(p.v[0], p.v[1], p.v[2])).toBeLessThan(kicked * 0.35);
    expect(Math.hypot(p.o[0], p.o[1], p.o[2])).toBeLessThan(0.2);
  });

  it("30 fps and 60 fps stay close under bass", () => {
    const a = run(2, 1 / 30, { lift: 1, spread: 0.3 });
    const b = run(2, 1 / 60, { lift: 1, spread: 0.3 });
    expect(Math.abs(a.y - b.y)).toBeLessThan(0.03);
    expect(Math.abs(a.r - b.r)).toBeLessThan(0.04);
  });
});

const STIR_VMAX_CHECK = 2.2;

describe("local fields replace the global +Y bowl kick", () => {
  it("does not assign the same +Y velocity to every lower-bowl sample", () => {
    const field = { ...spawnForce(3, 0, -0.8, 1), ox: -0.45, oy: -0.6, oz: 0, dx: 0, dy: 1, dz: 0, radius: 0.2, speed: 0 };
    const points: [number, number, number][] = [[-0.45, -0.6, 0], [0, -0.65, 0], [0.45, -0.6, 0]];
    const next = points.map((home) => stirIntegrate(rest(), home, { lift: 0, spread: 0, swirl: 0 }, [field], 1 / 60));
    expect(next[0]!.v[1]).toBeGreaterThan(next[2]!.v[1] * 2);
  });

  it("sustained sub energy raises the bowl rather than pressing it down", () => {
    let p = rest();
    for (let i = 0; i < 120; i += 1) {
      p = stirIntegrate(p, [0, -0.55, 0], { lift: 1, spread: 0, swirl: 0 }, emptyForces(), 1 / 60);
    }
    expect(p.o[1]).toBeGreaterThan(0.08);
    expect(p.o[1]).toBeLessThan(0.4);
  });
});

describe("continuous stereo plume", () => {
  it("keeps adjacent centre particles together during a centred field", () => {
    const field = { ...spawnForce(1, 0, 0, 1), ox: 0, oy: -0.55, oz: 0, dx: 0, dy: 1, dz: 0, radius: 0.45, speed: 0 };
    const xs = [-0.0001, 0, 0.0001];
    const velocities = xs.map((x) => stirIntegrate(rest(), [x, -0.55, 0], { lift: 0, spread: 1, swirl: 0 }, [field], 1 / 60).v[0]);
    expect(Math.max(...velocities.map(Math.abs))).toBeLessThan(0.02);
  });

  it("does not amplify existing lateral displacement into a split", () => {
    const field = { ...spawnForce(2, 0, 0, 1), ox: 0, oy: -0.55, oz: 0, dx: 0, dy: 1, dz: 0, radius: 0.4, speed: 0 };
    const shifted = { o: [0.2, 0, 0] as [number, number, number], v: [0, 0, 0] as [number, number, number] };
    const kick = stirIntegrate(shifted, [0, -0.55, 0], { lift: 0, spread: 1, swirl: 0 }, [field], 1 / 60);
    expect(Math.abs(kick.v[0])).toBeLessThan(Math.abs(kick.v[1]) + 0.2);
  });

  it("repeated wide kicks preserve a connected centre and settle after release", () => {
    const points = Array.from({ length: 101 }, (_, i) => ({
      home: [(i - 50) * 0.008, -0.55, 0] as [number, number, number],
      p: rest(),
    }));
    let maxGap = 0;
    let field: ForceSlot[] = emptyForces();
    for (let frame = 0; frame < 600; frame += 1) {
      if (frame % 12 === 0) {
        field = [ { ...spawnForce(frame + 1, 0, 0, 1), ox: 0, oy: -0.55, oz: 0, dx: 0, dy: 1, dz: 0, radius: 0.4, speed: 0, age: 0, life: 0.18 } ];
      }
      field = stepForces(field, 1 / 60);
      for (const point of points) {
        point.p = stirIntegrate(point.p, point.home, { lift: 0.4, spread: 1, swirl: 0 }, field, 1 / 60);
      }
      const xs = points.map((point) => point.home[0] + point.p.o[0]).sort((a, b) => a - b);
      maxGap = Math.max(maxGap, xs[51]! - xs[49]!);
    }
    expect(maxGap).toBeLessThan(0.05);
    for (let frame = 0; frame < 180; frame += 1) {
      for (const point of points) {
        point.p = stirIntegrate(point.p, point.home, { lift: 0, spread: 0, swirl: 0 }, emptyForces(), 1 / 60);
      }
    }
    expect(Math.max(...points.map((point) => Math.hypot(...point.p.o)))).toBeLessThan(0.16);
  });
});
