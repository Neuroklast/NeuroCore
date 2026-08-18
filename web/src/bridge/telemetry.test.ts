import { describe, expect, it } from "vitest";
import { createTelemetryViews, decodeTelemetry, peakToDb, TELEMETRY_MAGIC } from "./telemetry";

function writeF32(buf: DataView, off: number, v: number) {
  buf.setFloat32(off, v, true);
}

describe("decodeTelemetry", () => {
  it("reads NKTM header and copies scope samples", () => {
    const scopeN = 2;
    const gonioN = 1;
    const bytes = 32 + (scopeN * 2 + gonioN * 2) * 4;
    const ab = new ArrayBuffer(bytes);
    const v = new DataView(ab);
    v.setUint32(0, TELEMETRY_MAGIC, true);
    v.setUint16(4, 1, true);
    writeF32(v, 8, 0.8);
    writeF32(v, 12, 0.4);
    writeF32(v, 16, 0.2);
    writeF32(v, 20, 0.1);
    writeF32(v, 24, 0.5);
    v.setUint16(28, scopeN, true);
    v.setUint16(30, gonioN, true);
    const f = new Float32Array(ab, 32);
    f[0] = 0.1;
    f[1] = 0.2;
    f[2] = 0.3;
    f[3] = 0.4;
    f[4] = 0.5;
    f[5] = -0.5;

    const dest = createTelemetryViews();
    expect(decodeTelemetry(ab, dest)).toBe(true);
    expect(dest.inPeak).toBeCloseTo(0.8);
    expect(dest.outPeak).toBeCloseTo(0.4);
    expect(dest.scopeIn[0]).toBeCloseTo(0.1);
    expect(dest.scopeOut[1]).toBeCloseTo(0.4);
    expect(dest.gonioY[0]).toBeCloseTo(-0.5);
  });

  it("rejects bad magic", () => {
    const dest = createTelemetryViews();
    expect(decodeTelemetry(new ArrayBuffer(32), dest)).toBe(false);
  });

  it("peakToDb floors silence", () => {
    expect(peakToDb(0)).toBe(-96);
    expect(peakToDb(1)).toBeCloseTo(0);
  });
});
