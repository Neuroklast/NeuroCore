import { readFileSync } from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { describe, expect, it } from "vitest";
import { buildDust } from "./ballModel";
import { stirIntegrate, type StirP } from "./stirModel";

const rest = (): StirP => ({ o: [0, 0, 0], v: [0, 0, 0] });

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
  impulseAt = -1,
): { y: number; r: number; vmax: number } {
  const c = cloud(32);
  const y0 = meanY(c);
  let t = 0;
  let vmax = 0;
  let n = 0;
  while (t < seconds - 1e-9) {
    const hit = impulseAt >= 0 && t < impulseAt && t + dt >= impulseAt ? 1 : 0;
    for (const x of c) {
      x.p = stirIntegrate(x.p, x.home, drive, hit, dt);
      vmax = Math.max(vmax, Math.hypot(x.p.v[0], x.p.v[1], x.p.v[2]));
    }
    t += dt;
    n += 1;
  }
  expect(n).toBeGreaterThan(10);
  return { y: meanY(c) - y0, r: meanR(c), vmax };
}

describe("stir rest bias", () => {
  it("60s silence does not drift", () => {
    const hit = run(60, 1 / 60, { lift: 0, spread: 0 });
    expect(Math.abs(hit.y)).toBeLessThan(0.02);
    expect(hit.vmax).toBeLessThan(0.05);
  });

  it("60s bass pulses but does not pile at the top or the shell", () => {
    const hit = run(60, 1 / 60, { lift: 1, spread: 0.4 });
    expect(hit.y).toBeLessThan(0.08);
    expect(hit.y).toBeGreaterThan(-0.2);
    expect(hit.r).toBeLessThan(0.95);
  });

  it("swirl from rest starts motion without a +Y lift force", () => {
    const hit = run(1, 1 / 60, { lift: 0, spread: 0, swirl: 1 });
    expect(hit.vmax).toBeGreaterThan(0.05);
  });

  it("a single impulse kicks then returns", () => {
    const c = cloud(16);
    const home = c[0]!.home;
    let p = rest();
    p = stirIntegrate(p, home, { lift: 0, spread: 0 }, 1, 1 / 60);
    const kicked = Math.hypot(p.v[0], p.v[1], p.v[2]);
    expect(kicked).toBeGreaterThan(0.4);
    for (let i = 0; i < 90; i += 1) {
      p = stirIntegrate(p, home, { lift: 0, spread: 0 }, 0, 1 / 60);
    }
    expect(Math.hypot(p.v[0], p.v[1], p.v[2])).toBeLessThan(kicked * 0.2);
    expect(Math.hypot(p.o[0], p.o[1], p.o[2])).toBeLessThan(0.16);
  });

  it("30 fps and 60 fps stay close under bass", () => {
    const a = run(2, 1 / 30, { lift: 1, spread: 0.3 });
    const b = run(2, 1 / 60, { lift: 1, spread: 0.3 });
    expect(Math.abs(a.y - b.y)).toBeLessThan(0.03);
    expect(Math.abs(a.r - b.r)).toBeLessThan(0.04);
  });
});

describe("stir shader contract", () => {
  it("does not add lift as a continuous +Y force or reapply a smoothed kick", () => {
    const here = path.dirname(fileURLToPath(import.meta.url));
    const src = readFileSync(path.join(here, "BallGl.tsx"), "utf8");
    expect(src).not.toMatch(/acc\.y \+= uLiftForce/);
    expect(src).toContain("STIR_LIFT_BIAS");
    expect(src).toContain("uImpulse");
    expect(src).toContain("max(min(pace, 2.0), 0.35)");
    expect(src).toMatch(/onsetScan\(onset,\s*telemetry\.scopeIn/);
    expect(src).not.toMatch(/uniform4f\(\s*fieldU\.uHeat,\s*0,\s*0,\s*0,\s*0\)/);
    expect(src).not.toContain("SETTLE_SECONDS");
    expect(src).toContain("publishViz(");
  });
});
