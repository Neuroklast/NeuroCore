import { describe, expect, it } from "vitest";
import { routingFaceModel, routingMapPath, routingStems } from "./routingMap";

describe("calculated routing map", () => {
  it("splits one in to ChipSpec outs and joins two ins with mix", () => {
    const split = routingStems("split_ms", {});
    expect(split).toHaveLength(2);
    expect(split[0]!.x1).toBe(12);
    expect(split[0]!.x2).toBe(88);
    expect(split[0]!.y1).toBe(split[1]!.y1);
    expect(split[0]!.y2).not.toBe(split[1]!.y2);
    const join = routingStems("join", { mix: "0.25" });
    expect(join).toHaveLength(2);
    expect(join[0]!.amp).toBeCloseTo(0.75, 5);
    expect(join[1]!.amp).toBeCloseTo(0.25, 5);
    expect(join[0]!.y2).toBe(20);
    expect(routingStems("join", { mix: "0.2" })[1]!.amp)
      .not.toBe(routingStems("join", { mix: "0.8" })[1]!.amp);
    expect(routingMapPath("split_ms", {})).not.toBe(routingMapPath("join_ms", {}));
  });

  it("paints width as two lanes and msplit as three outs, without eval", () => {
    const wide = routingStems("widen", { width: "1.4" });
    const narrow = routingStems("width", { width: "0.7" });
    expect(wide).toHaveLength(2);
    expect(wide[0]!.amp).toBe(1);
    expect(wide[1]!.amp).toBeCloseTo(1, 5);
    expect(narrow[1]!.amp).toBeCloseTo(0.5, 5);
    expect(routingFaceModel("widen1", { width: "d" }).width).toBe(1);
    expect(routingFaceModel("xover1", { f1: "b" }).mode).toBe("msplit");
    expect(routingStems("msplit", {})).toHaveLength(3);
    expect(routingFaceModel("bus", { name: "Hall" }).name).toBe("hall");
  });
});
