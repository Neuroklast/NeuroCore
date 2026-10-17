import { describe, expect, it } from "vitest";
import {
  assertNoOverlap,
  assertNoUnexpectedWrap,
  assertStableHitbox,
  boxOf,
} from "../../e2e/layout";

describe("layout helpers", () => {
  it("flags overlap and wrap, and keeps a stable hitbox", () => {
    const a = { x: 0, y: 0, w: 10, h: 10 };
    const b = { x: 20, y: 0, w: 10, h: 10 };
    expect(() => assertNoOverlap(a, b, "ok")).not.toThrow();
    expect(() => assertNoOverlap(a, { x: 5, y: 0, w: 10, h: 10 }, "hit")).toThrow(/overlap/);
    expect(() => assertNoUnexpectedWrap({ scrollWidth: 10, clientWidth: 10 }, "fit")).not.toThrow();
    expect(() => assertNoUnexpectedWrap({ scrollWidth: 40, clientWidth: 10 }, "wide")).toThrow(/wrap/);
    const r = boxOf({ x: 1, y: 2, width: 3, height: 4 });
    expect(() => assertStableHitbox(r, { ...r }, "same")).not.toThrow();
    expect(() => assertStableHitbox(r, { ...r, x: 8 }, "moved")).toThrow(/moved/);
  });
});
