import { describe, expect, it } from "vitest";
import { applyCameraTransform, cameraMatrix, fitCamera, panCamera, worldFromScreen, zoomCamera } from "./boardCamera";
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

  it("pan is a translation of the matrix, not a store write", () => {
    const next = panCamera({ tx: 32, ty: 32, scale: 1 }, 10, -4);
    expect(cameraMatrix(next)).toBe("matrix(1, 0, 0, 1, 42, 28)");
  });

  it("zoom keeps the cursor world point fixed", () => {
    const c = { tx: 32, ty: 32, scale: 1 };
    const z = zoomCamera(c, 100, 80, 0.92);
    const a = worldFromScreen(c, 100, 80);
    const b = worldFromScreen(z, 100, 80);
    expect(Math.abs(a.x - b.x)).toBeLessThan(1e-6);
    expect(Math.abs(a.y - b.y)).toBeLessThan(1e-6);
  });

  it("applyCameraTransform writes the CSS matrix", () => {
    const world = { style: { transform: "" } };
    applyCameraTransform(world, { tx: 8, ty: 4, scale: 1 });
    expect(world.style.transform).toBe("matrix(1, 0, 0, 1, 8, 4)");
  });
});
