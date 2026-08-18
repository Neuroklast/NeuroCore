export const TELEMETRY_MAGIC = 0x4E4B544D;
export const SCOPE_N = 256;
export const GONIO_N = 128;

export interface TelemetryViews {
  inPeak: number;
  outPeak: number;
  inRms: number;
  outRms: number;
  cpu01: number;
  scopeN: number;
  gonioN: number;
  scopeIn: Float32Array;
  scopeOut: Float32Array;
  gonioX: Float32Array;
  gonioY: Float32Array;
}

export function createTelemetryViews(): TelemetryViews {
  return {
    inPeak: 0,
    outPeak: 0,
    inRms: 0,
    outRms: 0,
    cpu01: 0,
    scopeN: SCOPE_N,
    gonioN: GONIO_N,
    scopeIn: new Float32Array(SCOPE_N),
    scopeOut: new Float32Array(SCOPE_N),
    gonioX: new Float32Array(GONIO_N),
    gonioY: new Float32Array(GONIO_N),
  };
}

export function decodeTelemetry(ab: ArrayBuffer, dest: TelemetryViews): boolean {
  if (ab.byteLength < 32) {
    return false;
  }
  const view = new DataView(ab);
  if (view.getUint32(0, true) !== TELEMETRY_MAGIC) {
    return false;
  }
  dest.inPeak = view.getFloat32(8, true);
  dest.outPeak = view.getFloat32(12, true);
  dest.inRms = view.getFloat32(16, true);
  dest.outRms = view.getFloat32(20, true);
  dest.cpu01 = view.getFloat32(24, true);
  dest.scopeN = view.getUint16(28, true);
  dest.gonioN = view.getUint16(30, true);
  const nS = Math.min(dest.scopeN, dest.scopeIn.length);
  const nG = Math.min(dest.gonioN, dest.gonioX.length);
  const f32 = new Float32Array(ab, 32);
  dest.scopeIn.set(f32.subarray(0, nS));
  dest.scopeOut.set(f32.subarray(nS, nS + nS));
  dest.gonioX.set(f32.subarray(nS + nS, nS + nS + nG));
  dest.gonioY.set(f32.subarray(nS + nS + nG, nS + nS + nG + nG));
  return true;
}

export function peakToDb(p: number): number {
  if (! Number.isFinite(p) || p <= 1.0e-8) {
    return -96;
  }
  return 20 * Math.log10(p);
}
