import { describe, expect, it } from "vitest";
import {
  FORCE_SLOTS,
  emptyForces,
  forceAccel,
  forceInfluence,
  packForceUniforms,
  pushForce,
  spawnForce,
  stepForces,
} from "./forceField";
import { rest, stirIntegrate } from "./stirModel";

describe("localized force fields", () => {
  it("a local impulse displaces a particle similarly at 30 and 60 fps", () => {
    const seed = { ...spawnForce(1, 0, 0, 1), ox: 0, oy: -0.5, oz: 0, dx: 0, dy: 1, dz: 0, radius: 0.5, speed: 0, life: 0.18, age: 0 };
    const run = (dt: number) => {
      let p = rest();
      let slots = [{ ...seed }];
      for (let t = 0; t < 0.22; t += dt) {
        p = stirIntegrate(p, [0, -0.5, 0], { lift: 0, spread: 0 }, slots, dt);
        slots = stepForces(slots, dt);
      }
      return p.o[1];
    };
    expect(Math.abs(run(1 / 30) - run(1 / 60))).toBeLessThan(0.08);
  });

  it("identical seeds spawn identical fields", () => {
    const a = spawnForce(42, 0, 0, 0.8);
    const b = spawnForce(42, 0, 0, 0.8);
    expect(a).toEqual(b);
    expect(spawnForce(43, 0, 0, 0.8).ox).not.toBe(a.ox);
  });

  it("nearby particles receive more acceleration than distant ones", () => {
    const slot = spawnForce(1, 0, 0, 1);
    slot.ox = 0;
    slot.oy = -0.5;
    slot.oz = 0;
    slot.dx = 0;
    slot.dy = 1;
    slot.dz = 0;
    slot.radius = 0.25;
    const near = forceInfluence(0, -0.5, 0, slot);
    const far = forceInfluence(0.8, 0.6, 0, slot);
    expect(near).toBeGreaterThan(far * 4);
    const an = forceAccel(0, -0.5, 0, [slot]);
    const af = forceAccel(0.8, 0.6, 0, [slot]);
    expect(an[1]).toBeGreaterThan(af[1] * 4);
  });

  it("a travelling front reaches a far region later", () => {
    const slot = { ...spawnForce(9, 0, 0, 1), ox: -0.6, oy: 0, oz: 0, dx: 1, dy: 0, dz: 0, speed: 2, radius: 0.2, life: 1, age: 0 };
    const at = (age: number, x: number) => forceInfluence(x, 0, 0, { ...slot, age });
    expect(at(0, -0.6)).toBeGreaterThan(at(0, 0.4));
    expect(at(0.5, 0.4)).toBeGreaterThan(at(0, 0.4));
  });

  it("expired slots pack as zero kick so they cannot re-fire", () => {
    let slots = emptyForces();
    slots = pushForce(slots, { ...spawnForce(2, 0, 0, 1), life: 0.05, age: 0 });
    expect(packForceUniforms(slots).kick[1]).toBeGreaterThan(0);
    slots = stepForces(slots, 0.2);
    expect(slots.every((s) => s.strength === 0)).toBe(true);
    expect(packForceUniforms(slots).kick.every((v) => v === 0)).toBe(true);
  });

  it("bounded slots never grow past FORCE_SLOTS", () => {
    let slots = emptyForces();
    for (let i = 1; i <= 12; i += 1) {
      slots = pushForce(slots, spawnForce(i, 0, 0, 1));
    }
    expect(slots).toHaveLength(FORCE_SLOTS);
  });
});

describe("stir uses local fields, not a global +Y kick", () => {
  it("a mono field at the left bowl does not lift a right-side particle as hard", () => {
    const field = { ...spawnForce(8, 0, -1, 1), ox: -0.45, oy: -0.6, oz: 0, dx: 0, dy: 1, dz: 0, radius: 0.22, speed: 0 };
    const left = stirIntegrate(rest(), [-0.45, -0.6, 0], { lift: 0, spread: 0, swirl: 0 }, [field], 1 / 60);
    const right = stirIntegrate(rest(), [0.45, -0.6, 0], { lift: 0, spread: 0, swirl: 0 }, [field], 1 / 60);
    expect(left.v[1]).toBeGreaterThan(right.v[1] * 3);
    expect(Math.abs(left.v[1] - right.v[1])).toBeGreaterThan(0.1);
  });

  it("silence with empty fields does not apply a kick", () => {
    const p = stirIntegrate(rest(), [0, -0.55, 0], { lift: 0, spread: 0, swirl: 0 }, emptyForces(), 1 / 60);
    expect(Math.hypot(p.v[0], p.v[1], p.v[2])).toBeLessThan(0.08);
  });

  it("held rest-bias lift still raises the bowl without a one-shot", () => {
    let p = rest();
    for (let i = 0; i < 120; i += 1) {
      p = stirIntegrate(p, [0, -0.55, 0], { lift: 1, spread: 0, swirl: 0 }, emptyForces(), 1 / 60);
    }
    expect(p.o[1]).toBeGreaterThan(0.08);
    expect(p.o[1]).toBeLessThan(0.4);
  });

  it("centre particles stay together under a centred low field", () => {
    const field = { ...spawnForce(1, 0, 0, 1), ox: 0, oy: -0.55, oz: 0, dx: 0, dy: 1, dz: 0, radius: 0.4, speed: 0 };
    const xs = [-0.0001, 0, 0.0001].map((x) =>
      stirIntegrate(rest(), [x, -0.55, 0], { lift: 0, spread: 1, swirl: 0 }, [field], 1 / 60).v[0],
    );
    expect(Math.max(...xs.map(Math.abs))).toBeLessThan(0.02);
  });
});
