import { describe, expect, it } from "vitest";
import { shouldPlayBoot } from "./boot";

describe("boot motion", () => {
  it("plays unless the user prefers reduced motion", () => {
    expect(shouldPlayBoot(false)).toBe(true);
    expect(shouldPlayBoot(true)).toBe(false);
    expect(shouldPlayBoot(false, "off")).toBe(false);
    expect(shouldPlayBoot(false, "reduced")).toBe(false);
  });
});
