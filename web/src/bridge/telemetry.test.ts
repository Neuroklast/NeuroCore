import { describe, expect, it } from "vitest";
import { readFileSync } from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { createTelemetryViews, decodeTelemetry, gonioCorr, peakToDb, TELEMETRY_MAGIC } from "./telemetry";
import { stereoMetrics } from "../face/faceModel";

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
    expect(dest.seq).toBe(0);
  });

  it("reads v3 onset events after the v2 tail", () => {
    const scopeN = 1;
    const gonioN = 1;
    const body = 32 + (scopeN * 2 + gonioN * 2) * 4;
    const list = body + 8;
    const ab = new ArrayBuffer(list + 4 + 24);
    const v = new DataView(ab);
    v.setUint32(0, TELEMETRY_MAGIC, true);
    v.setUint16(4, 3, true);
    writeF32(v, 8, 0.2);
    writeF32(v, 12, 0.1);
    writeF32(v, 16, 0.05);
    writeF32(v, 20, 0.04);
    writeF32(v, 24, 0.1);
    v.setUint16(28, scopeN, true);
    v.setUint16(30, gonioN, true);
    v.setUint32(body, 9, true);
    v.setUint32(body + 4, 100, true);
    v.setUint16(list, 2, true);
    v.setUint32(list + 4, 40, true);
    writeF32(v, list + 8, 0.7);
    writeF32(v, list + 12, -1);
    v.setUint32(list + 16, 4000, true);
    writeF32(v, list + 20, 0.4);
    writeF32(v, list + 24, 1);
    const dest = createTelemetryViews();
    expect(decodeTelemetry(ab, dest)).toBe(true);
    expect(dest.seq).toBe(9);
    expect(dest.onsets).toHaveLength(2);
    expect(dest.onsets[0]?.samplePos).toBe(40);
    expect(dest.onsets[0]?.amp).toBeCloseTo(0.7);
    expect(dest.onsets[0]?.balance).toBeCloseTo(-1);
    expect(dest.onsets[1]?.samplePos).toBe(4000);
    expect(dest.onsets[1]?.balance).toBeCloseTo(1);
  });

  it("reads v2 seq and sample position after the payload", () => {
    const scopeN = 1;
    const gonioN = 1;
    const body = 32 + (scopeN * 2 + gonioN * 2) * 4;
    const ab = new ArrayBuffer(body + 8);
    const v = new DataView(ab);
    v.setUint32(0, TELEMETRY_MAGIC, true);
    v.setUint16(4, 2, true);
    writeF32(v, 8, 0.2);
    writeF32(v, 12, 0.1);
    writeF32(v, 16, 0.05);
    writeF32(v, 20, 0.04);
    writeF32(v, 24, 0.1);
    v.setUint16(28, scopeN, true);
    v.setUint16(30, gonioN, true);
    v.setUint32(body, 17, true);
    v.setUint32(body + 4, 4096, true);
    const dest = createTelemetryViews();
    expect(decodeTelemetry(ab, dest)).toBe(true);
    expect(dest.seq).toBe(17);
    expect(dest.samplePos).toBe(4096);
  });

  it("rejects bad magic", () => {
    const dest = createTelemetryViews();
    expect(decodeTelemetry(new ArrayBuffer(32), dest)).toBe(false);
  });

  it("peakToDb floors silence", () => {
    expect(peakToDb(0)).toBe(-60);
    expect(peakToDb(0.00001)).toBe(-60);
    expect(peakToDb(Number.NaN)).toBe(-60);
    expect(peakToDb(1)).toBeCloseTo(0);
  });
});

it("rejects truncated telemetry without changing the last valid frame", () => {
  const dest = createTelemetryViews();
  dest.inPeak = 0.5;
  const ab = new ArrayBuffer(36);
  const header = new DataView(ab);
  header.setUint32(0, TELEMETRY_MAGIC, true);
  header.setUint16(4, 1, true);
  header.setUint16(28, 256, true);
  expect(decodeTelemetry(ab, dest)).toBe(false);
  expect(dest.inPeak).toBe(0.5);
});

it("decodes peaks without allocating a mapped readings array", () => {
  const here = path.dirname(fileURLToPath(import.meta.url));
  const src = readFileSync(path.join(here, "telemetry.ts"), "utf8");
  const body = src.slice(src.indexOf("export function decodeTelemetry"), src.indexOf("export function gonioCorr"));
  expect(body).not.toContain(".map(");
});

it("gonioCorr matches stereoMetrics.corr", () => {
  const x = new Float32Array([0.1, -0.2, 0.05]);
  const y = new Float32Array([-0.4, 0.1, 0.2]);
  expect(gonioCorr(x, y)).toBeCloseTo(stereoMetrics(x, y).corr, 10);
});

it("clears unused scope samples when a shorter valid frame arrives", () => {
  const dest = createTelemetryViews();
  dest.scopeIn.fill(0.8);
  const ab = new ArrayBuffer(40);
  const header = new DataView(ab);
  header.setUint32(0, TELEMETRY_MAGIC, true);
  header.setUint16(4, 1, true);
  header.setUint16(28, 1, true);
  expect(decodeTelemetry(ab, dest)).toBe(true);
  expect(dest.scopeIn[20]).toBe(0);
});
