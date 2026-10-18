import { describe, expect, it } from "vitest";
import { BOARD_MAX_SCALE, BOARD_MIN_SCALE } from "../boardModel";
import { chipBox, jackAnchor } from "../chipLayout";
import { handleId } from "../handles";
import { visualJacksFor } from "../visualEdges";
import {
  SEMANTIC_ZOOM,
  semanticZoomLevel,
  zoomMayMovePorts,
  zoomMayResizeChip,
} from "./semanticZoom";

describe("semantic zoom", () => {
  it("splits the existing camera range into overview / normal / focused", () => {
    expect(SEMANTIC_ZOOM.min).toBe(BOARD_MIN_SCALE);
    expect(SEMANTIC_ZOOM.max).toBe(BOARD_MAX_SCALE);
    expect(semanticZoomLevel(0.4)).toBe("overview");
    expect(semanticZoomLevel(0.54)).toBe("overview");
    expect(semanticZoomLevel(0.7)).toBe("normal");
    expect(semanticZoomLevel(0.85)).toBe("focused");
    expect(semanticZoomLevel(1)).toBe("focused");
    expect(zoomMayMovePorts("overview")).toBe(false);
    expect(zoomMayResizeChip("focused")).toBe(false);
  });

  it("keeps jack centres on the chip box at every zoom level", () => {
    const jacks = visualJacksFor({
      id: "filter1",
      type: "filter",
      busName: "main",
      args: { type: "lowpass", cutoff: "900" },
      trailingComment: "",
      jacks: [],
    });
    const box = chipBox("filter", jacks, false, { type: "lowpass", cutoff: "900" });
    const a = jackAnchor({ x: 64, y: 96 }, "filter", jacks, handleId("out", true), true, box.h, box.w);
    const b = jackAnchor({ x: 64, y: 96 }, "filter", jacks, handleId("in", false), false, box.h, box.w);
    for (const level of ["overview", "normal", "focused"] as const) {
      expect(zoomMayMovePorts(level), level).toBe(false);
      expect(a.x, level).toBe(64 + box.w);
      expect(b.x, level).toBe(64);
      expect(box.w % 32, level).toBe(0);
      expect(box.h % 32, level).toBe(0);
    }
  });
});
