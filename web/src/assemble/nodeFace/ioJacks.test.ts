import { describe, expect, it } from "vitest";
import { ioFaceModel, ioJackDots, ioJackPath } from "./ioJacks";

describe("calculated io jacks", () => {
  it("places IN sources east and OUT mix-ins west from ChipSpec", () => {
    expect(ioFaceModel("in").outs).toEqual(["out", "sc"]);
    expect(ioFaceModel("in").ins).toEqual([]);
    const inn = ioJackDots("in");
    expect(inn).toHaveLength(2);
    expect(inn.every((d) => d.east)).toBe(true);
    expect(inn.map((d) => d.id)).toEqual(["out", "sc"]);
    expect(inn[0]!.y).not.toBe(inn[1]!.y);
    const out = ioJackDots("out");
    expect(out).toHaveLength(1);
    expect(out[0]!.id).toBe("in");
    expect(out[0]!.east).toBe(false);
    expect(ioJackPath("in")).not.toBe(ioJackPath("out"));
    expect(ioJackDots("sidechain")).toEqual([
      expect.objectContaining({ id: "out", east: true }),
    ]);
  });
});
