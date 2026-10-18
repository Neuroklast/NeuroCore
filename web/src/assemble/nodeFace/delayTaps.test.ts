import { describe, expect, it } from "vitest";
import { FACE_BPM } from "./faceArgs";
import { delayFaceModel, delayTaps, delayTapsPath } from "./delayTaps";
import { wholeToMs } from "../../chrome/noteValue";

describe("calculated delay taps", () => {
  it("moves echo spacing when time changes, and feedback grows later taps", () => {
    const short = delayTaps({ time: "250", feedback: "0.5", mix: "0.4" });
    const long = delayTaps({ time: "500", feedback: "0.5", mix: "0.4" });
    expect(short[1]!.t).toBe(250);
    expect(long[1]!.t).toBe(500);
    expect(delayTapsPath({ time: "250", feedback: "0.5", mix: "0.4" }))
      .not.toBe(delayTapsPath({ time: "500", feedback: "0.5", mix: "0.4" }));
    const bright = delayTaps({ time: "250", feedback: "0.8", mix: "0.4" });
    const dull = delayTaps({ time: "250", feedback: "0.2", mix: "0.4" });
    expect(bright[2]!.amp).toBeGreaterThan(dull[2]!.amp);
  });

  it("uses host-default BPM when sync is a note, and does not eval formulas", () => {
    expect(delayFaceModel({ sync: "1/4", time: "250", mix: "0.3" }).timeMs)
      .toBeCloseTo(wholeToMs(0.25, FACE_BPM), 5);
    expect(delayFaceModel({ sync: "1/4", time: "250" }).synced).toBe(true);
    expect(delayFaceModel({ time: "b", feedback: "0.2" }).timeMs).toBe(250);
    expect(delayTaps({ time: "250", mix: "0.3" })[0]!.amp).toBeCloseTo(0.7, 5);
  });
});
