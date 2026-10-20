import { expect, it } from "vitest";
import { advanceCableSignal, emptyCableSignal } from "./cableSignal";
it("consumes a tap snapshot once and expires packets rather than wrapping", () => {
  const token = {},
    s = emptyCableSignal();
  advanceCableSignal(s, token, [0], 0.2, 0.8, 64, 0.01);
  expect(s.packets.length).toBe(1);
  for (let i = 0; i < 200; i++)
    advanceCableSignal(s, token, [0], 0.2, 0.8, 64, 0.01);
  expect(s.packets.length).toBe(0);
  expect(s.freshness).toBe(0);
});
it("silence and missing measurements create no energy or packets", () => {
  const s = emptyCableSignal();
  advanceCableSignal(s, {}, [], 0, 0, 200, 0.016);
  expect(s.packets).toEqual([]);
});
it("bounds packet work during dense fresh telemetry", () => {
  const s = emptyCableSignal();
  for (let i = 0; i < 200; i++)
    advanceCableSignal(
      s,
      {},
      [0.1, 0.2, 0.3, 0.4, 0.5, 0.6],
      0.5,
      1,
      20000,
      0.016,
    );
  expect(s.packets.length).toBeLessThanOrEqual(12);
});

it("emits one impulse at the source regardless of tap peak positions", () => {
  const s = emptyCableSignal();
  advanceCableSignal(s, {}, [0.3, 0.8], 0.2, 0.8, 1000, 0);
  expect(s.packets).toEqual([0]);
});
it("lets emitted impulses travel and fade after silence", () => {
  const s = emptyCableSignal();
  advanceCableSignal(s, {}, [0.5], 0.2, 0.8, 1000, 0);
  advanceCableSignal(s, {}, [], 0, 0, 1000, 0.05);
  expect(s.packets.length).toBe(1);
  expect(s.packets[0]).toBeGreaterThan(0);
});
it("does not create packets while animation is disabled", () => {
  const s = emptyCableSignal();
  advanceCableSignal(s, {}, [0.5], 0.2, 0.8, 1000, 0.016, false);
  expect(s.packets).toEqual([]);
});
