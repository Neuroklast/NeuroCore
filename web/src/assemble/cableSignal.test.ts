import { measureCablePath, cablePointAt, traceCableSpan } from "./cablePaint";
import { expect, it } from "vitest";
import { advanceCableSignal, emptyCableSignal, cablePacketAlpha } from "./cableSignal";
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

it("smooths a sudden speed change instead of jumping to the target", () => {
  const s = emptyCableSignal();
  advanceCableSignal(s, {}, [0], 0.01, 0.02, 1000, 0);
  const before = s.speed;
  advanceCableSignal(s, {}, [0], 1, 1, 1000, 0.016);
  expect(s.speed).toBeGreaterThan(before);
  expect(s.speed).toBeLessThan(144);
});
it("fades a packet continuously before its lifetime ends", () => {
  expect(cablePacketAlpha({ distance: 10, age: 1.7, strength: 1 })).toBeCloseTo(0.5);
  expect(cablePacketAlpha({ distance: 10, age: 2, strength: 1 })).toBe(0);
});

it("keeps impulse segments on every corner of the cached route", () => {
  const path = measureCablePath([{ x: 0, y: 0 }, { x: 10, y: 0 }, { x: 10, y: 10 }]);
  expect(path.length).toBe(20);
  expect(cablePointAt(path, 15)).toEqual({ x: 10, y: 5 });
  const points: number[][] = [];
  const ctx = {
    moveTo: (x: number, y: number) => points.push([x, y]),
    lineTo: (x: number, y: number) => points.push([x, y]),
  } as unknown as CanvasRenderingContext2D;
  traceCableSpan(ctx, path, 5, 15);
  expect(points).toEqual([[5, 0], [10, 0], [10, 5]]);
});

it("carries a measured impulse to the end of a long route", () => {
  const s = emptyCableSignal();
  advanceCableSignal(s, {}, [0.5], 0.3, 0.7, 1800, 0);
  const token = {};
  for (let i = 0; i < 150; i++) advanceCableSignal(s, token, [], 0.3, 0.7, 1800, 0.02);
  expect(s.impulses.length).toBeGreaterThan(0);
  expect(s.impulses[0]!.distance).toBeGreaterThan(0);
});

it("represents sustained measured energy even without a local tap maximum", () => {
  const s = emptyCableSignal();
  advanceCableSignal(s, {}, [], 0.2, 0.2, 500, 0.016);
  expect(s.impulses.length).toBe(1);
});
