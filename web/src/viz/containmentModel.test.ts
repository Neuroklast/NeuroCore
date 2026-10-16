import { readFileSync } from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { describe, expect, it } from "vitest";
import { themeIds, themeOf } from "../theme/theme";
import { BALL_STATIC, ballDrive, ballRadius, type BallDrive } from "./ballModel";
import {
  CLAMP_ANGLES,
  CLAMP_COUNT,
  CONTAINMENT_STATIC,
  EDGE_MARGIN,
  OUTER_K_MAX,
  OUTER_K_MIN,
  PULSE_MAX,
  RING_COUNT,
  RING_OMEGA,
  circleHitsAabb,
  containmentLayout,
  containmentPalette,
  containmentPose,
  orbitShards,
  teleBoxes,
} from "./containmentModel";

const PANE = { w: 1256, h: 500 };

const loud: BallDrive = {
  spread: 0.8,
  lift: 1,
  kick: 1,
  glow: 0.5,
  twinkle: 0.9,
  swirl: 0.8,
  size: 1,
  dustSpeed: 1,
  still: false,
};

function runPose(motion: "full" | "reduced" | "off", drive: BallDrive, seconds: number): ReturnType<typeof containmentPose> {
  let p = containmentPose(null, drive, 1 / 60, 0, motion);
  const n = Math.round(seconds * 60);
  for (let i = 1; i <= n; i += 1) {
    p = containmentPose(p, drive, 1 / 60, i / 60, motion);
  }
  return p;
}

describe("containment layout", () => {
  it("keeps the outer ring in 1.10–1.23 of ball radius with a 20px edge margin", () => {
    const L = containmentLayout(PANE.w, PANE.h);
    const ratio = L.outerR / L.ballR;
    expect(L.ballR).toBe(ballRadius(PANE.w, PANE.h));
    expect(ratio).toBeGreaterThanOrEqual(OUTER_K_MIN - 1e-6);
    expect(ratio).toBeLessThanOrEqual(OUTER_K_MAX + 1e-6);
    expect(PANE.h / 2 - L.outerR).toBeGreaterThanOrEqual(EDGE_MARGIN - 1e-6);
    expect(PANE.w / 2 - L.outerR).toBeGreaterThanOrEqual(EDGE_MARGIN - 1e-6);
  });

  it("shrinks to keep the 20px margin on a short pane, even if the ratio drops", () => {
    const L = containmentLayout(800, 160);
    expect(160 / 2 - L.outerR).toBeGreaterThanOrEqual(EDGE_MARGIN - 1e-6);
    expect(L.outerR).toBeGreaterThan(L.ballR);
  });

  it("places four layered clamps on the diagonals and three unequal rings with gaps", () => {
    const L = containmentLayout(PANE.w, PANE.h);
    expect(CLAMP_ANGLES).toHaveLength(CLAMP_COUNT);
    expect(CLAMP_ANGLES.map((a) => Math.round((a * 180) / Math.PI))).toEqual([45, 135, 225, 315]);
    expect(L.rings).toHaveLength(RING_COUNT);
    expect(L.rings[0]!.bright).toBeGreaterThan(L.rings[1]!.bright);
    expect(L.rings[1]!.bright).toBeGreaterThan(L.rings[2]!.bright);
    for (const ring of L.rings) {
      expect(ring.gap).toBeGreaterThan(0.05);
      expect(ring.gap).toBeLessThan(0.5);
      expect(ring.segments).toBeGreaterThanOrEqual(2);
    }
    expect(L.rings[0]!.r).toBeLessThan(L.rings[1]!.r);
    expect(L.rings[1]!.r).toBeLessThan(L.rings[2]!.r);
    expect(L.clamp.jawX).toBeGreaterThan(L.ballR * 0.95);
    expect(L.clamp.jawX).toBeLessThan(L.clamp.splitX);
    expect(L.clamp.splitX).toBeLessThan(L.clamp.yokeX);
    expect(L.clamp.yokeHy).toBeGreaterThan(L.clamp.armHy);
    expect(L.clamp.pipX).toBeLessThan(L.clamp.yokeX);
  });

  it("does not let the outer ring cover the tele HUD boxes", () => {
    const L = containmentLayout(PANE.w, PANE.h);
    for (const box of teleBoxes(PANE.w, PANE.h)) {
      expect(circleHitsAabb(L.cx, L.cy, L.outerR, box)).toBe(false);
    }
  });

  it("places shards on the projected ellipse the rails use", () => {
    const L = containmentLayout(PANE.w, PANE.h);
    const shards = orbitShards(L, 0);
    let i = 0;
    for (const orbit of L.orbits) {
      const ry = orbit.ry * Math.cos(orbit.tilt);
      for (let k = 0; k < orbit.count; k += 1) {
        const s = shards[i++]!;
        const nx = (s.x - L.cx) / orbit.rx;
        const ny = (s.y - L.cy) / ry;
        expect(Math.hypot(nx, ny)).toBeCloseTo(1, 5);
      }
    }
  });

  it("splits orbit shards in front of and behind the core, inside the pane", () => {
    const L = containmentLayout(PANE.w, PANE.h);
    const shards = orbitShards(L, CONTAINMENT_STATIC.orbitPhase);
    expect(shards.length).toBeGreaterThanOrEqual(12);
    expect(shards.some((s) => s.z < 0)).toBe(true);
    expect(shards.some((s) => s.z > 0)).toBe(true);
    for (const s of shards) {
      expect(s.x).toBeGreaterThan(0);
      expect(s.x).toBeLessThan(PANE.w);
      expect(s.y).toBeGreaterThan(0);
      expect(s.y).toBeLessThan(PANE.h);
    }
  });
});

describe("containment pose", () => {
  it("motion off is the rest frame, even under loud drive", () => {
    const hit = containmentPose(null, loud, 1 / 60, 8, "off");
    expect(hit).toEqual(CONTAINMENT_STATIC);
    expect(containmentPose(hit, loud, 1 / 60, 9, "off")).toEqual(CONTAINMENT_STATIC);
  });

  it("rings rotate opposite ways; reduced is slower; pulse stays within 2% of radius", () => {
    expect(Math.sign(RING_OMEGA[0]!)).not.toBe(Math.sign(RING_OMEGA[1]!));
    const full = runPose("full", loud, 2);
    const reduced = runPose("reduced", loud, 2);
    expect(full.still).toBe(false);
    expect(Math.abs(full.ringRot[0]! - CONTAINMENT_STATIC.ringRot[0]!))
      .toBeGreaterThan(Math.abs(reduced.ringRot[0]! - CONTAINMENT_STATIC.ringRot[0]!));
    expect(full.pulse).toBeLessThanOrEqual(PULSE_MAX + 1e-6);
    expect(full.pulse).toBeGreaterThan(0.005);
    expect(reduced.pulse).toBeLessThan(full.pulse);
  });

  it("silence has no discharges; a high-band drive can spark a clamp", () => {
    const quiet = runPose("full", BALL_STATIC, 1);
    expect(quiet.discharge.every((d) => d === 0)).toBe(true);
    const hot = runPose("full", loud, 2);
    expect(hot.discharge.some((d) => d > 0)).toBe(true);
  });

  it("pulse decays inside 80–160 ms after the kick drops", () => {
    let p = containmentPose(null, loud, 1 / 60, 0, "full");
    for (let i = 1; i < 30; i += 1) {
      p = containmentPose(p, loud, 1 / 60, i / 60, "full");
    }
    expect(p.pulse).toBeGreaterThan(0.008);
    const after = runFrom(p, BALL_STATIC, 0.16);
    expect(after.pulse).toBeLessThan(p.pulse * 0.45);
  });
});

function runFrom(start: ReturnType<typeof containmentPose>, drive: BallDrive, seconds: number) {
  let p = start;
  const n = Math.round(seconds * 60);
  for (let i = 1; i <= n; i += 1) {
    p = containmentPose(p, drive, 1 / 60, i / 60, "full");
  }
  return p;
}

describe("containment palette", () => {
  it("derives metal and accent from each theme, not a private red", () => {
    for (const id of themeIds()) {
      const pal = containmentPalette(themeOf(id));
      const accent = pal.accent;
      expect(accent[0]).toBeGreaterThanOrEqual(0);
      expect(accent[0]).toBeLessThanOrEqual(1);
      expect(pal.cyan[2]).toBeGreaterThan(pal.cyan[0]! * 0.5);
      const lo = pal.metalLo[0]! + pal.metalLo[1]! + pal.metalLo[2]!;
      const hi = pal.metalHi[0]! + pal.metalHi[1]! + pal.metalHi[2]!;
      expect(lo).toBeLessThan(hi);
    }
    const signal = containmentPalette(themeOf("signal"));
    const gold = containmentPalette(themeOf("gold"));
    expect(signal.accent[0]).toBeGreaterThan(signal.accent[1]!);
    expect(gold.accent[0]).toBeGreaterThan(0.7);
    expect(gold.accent[1]).toBeGreaterThan(0.7);
  });

  it("does not hardcode Signal red in containment sources", () => {
    const here = path.dirname(fileURLToPath(import.meta.url));
    const blob = ["containmentModel.ts", "containmentPass.ts"]
      .map((f) => readFileSync(path.join(here, f), "utf8"))
      .join("\n");
    expect(blob).not.toMatch(/#c42a2a/i);
    expect(blob).not.toMatch(/196\s*,\s*42\s*,\s*42/);
  });
});

describe("ballDrive stays the source of truth", () => {
  it("containment consumes ballDrive and does not replace it", () => {
    const d = ballDrive(null, { low: 1, mid: 0, high: 0 }, 0.4, 0, 1 / 60, { motion: "full", transient: true });
    const pose = containmentPose(null, d, 1 / 60, 0.1, "full");
    expect(d.kick).toBeGreaterThan(0.3);
    expect(pose.pulse).toBeGreaterThan(0);
  });
});
