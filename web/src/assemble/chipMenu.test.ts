import { describe, expect, it } from "vitest";
import { chipMenuActions } from "./chipMenu";

describe("chip context menu", () => {
  it("offers insert, inspect, delete — not mute/solo or a second details entry", () => {
    expect(chipMenuActions()).toEqual(["insertAfter", "inspect", "delete"]);
    expect(chipMenuActions()).not.toContain("mute");
    expect(chipMenuActions()).not.toContain("details");
  });
});