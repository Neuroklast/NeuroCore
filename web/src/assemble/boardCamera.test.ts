import { describe, expect, it } from "vitest";
import { fitCamera, worldFromScreen } from "./boardCamera";
import { BOARD_MIN_SCALE, type BoardNode } from "./boardModel";

function node(id: string, x: number, y: number, w: number, h: number): BoardNode {
  return { id, type: "stage", role: "chip", x, y, w, h, args: {}, label: id, channel: "", locked: false };
}

describe("board camera", () => {
  it("fits the chain into the pane without reading the DOM", () => {
    const cam = fitCamera([
      node("IN", 32, 32, 128, 96),
      node("OUT", 800, 32, 128, 96),
    ], { w: 960, h: 420 });
    expect(cam.scale).toBeGreaterThanOrEqual(BOARD_MIN_SCALE);
    expect(cam.scale).toBeLessThanOrEqual(1);
    const a = worldFromScreen(cam, cam.tx + 32 * cam.scale, cam.ty + 32 * cam.scale);
    expect(Math.abs(a.x - 32)).toBeLessThan(1);
    expect(Math.abs(a.y - 32)).toBeLessThan(1);
  });
});
