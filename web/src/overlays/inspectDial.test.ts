import { readFileSync } from "node:fs";
import { describe, expect, it } from "vitest";
import {
  INSPECT_DIAL_CX,
  INSPECT_DIAL_CY,
  INSPECT_DIAL_R,
  INSPECT_DIAL_VIEW,
} from "./inspectDial";

describe("inspect dial geometry", () => {
  it("keeps the arc inside the 44 viewBox", () => {
    expect(INSPECT_DIAL_CX - INSPECT_DIAL_R).toBeGreaterThanOrEqual(0);
    expect(INSPECT_DIAL_CY - INSPECT_DIAL_R).toBeGreaterThanOrEqual(0);
    expect(INSPECT_DIAL_CX + INSPECT_DIAL_R).toBeLessThanOrEqual(INSPECT_DIAL_VIEW);
    expect(INSPECT_DIAL_CY + INSPECT_DIAL_R).toBeLessThanOrEqual(INSPECT_DIAL_VIEW);
  });

  it("InspectKnob paints that dial, not the footer origin", () => {
    const src = readFileSync(new URL("./InspectKnob.tsx", import.meta.url), "utf8");
    expect(src).toContain("INSPECT_DIAL_CX");
    expect(src).not.toMatch(/KNOB_CX/);
  });
});
