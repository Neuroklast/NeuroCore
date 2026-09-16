export const TELEMETRY_MAGIC = 0x4E4B544D;
export const SCOPE_N = 256;
export const GONIO_N = 128;
export const DISPLAY_DB_FLOOR = -60;
export const DISPLAY_AMP_FLOOR = 10 ** (DISPLAY_DB_FLOOR / 20);

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
  const scopeN = view.getUint16(28, true);
  const gonioN = view.getUint16(30, true);
  const requiredBytes = 32 + (scopeN * 2 + gonioN * 2) * 4;
  if (view.getUint16(4, true) !== 1 || ab.byteLength < requiredBytes) return false;
  const readings = [8, 12, 16, 20, 24].map((offset) => view.getFloat32(offset, true));
  if (readings.some((value) => !Number.isFinite(value) || value < 0)) return false;
  [dest.inPeak, dest.outPeak, dest.inRms, dest.outRms, dest.cpu01] = readings as [number, number, number, number, number];
  dest.scopeN = Math.min(scopeN, dest.scopeIn.length);
  dest.gonioN = Math.min(gonioN, dest.gonioX.length);
  const f32 = new Float32Array(ab, 32, scopeN * 2 + gonioN * 2);
  const copy = (target: Float32Array, offset: number, length: number) => {
    target.fill(0);
    for (let i = 0; i < length; i += 1) {
      const value = f32[offset + i]!;
      target[i] = Number.isFinite(value) ? value : 0;
    }
  };
  copy(dest.scopeIn, 0, dest.scopeN);
  copy(dest.scopeOut, scopeN, dest.scopeN);
  copy(dest.gonioX, scopeN * 2, dest.gonioN);
  copy(dest.gonioY, scopeN * 2 + gonioN, dest.gonioN);
  return true;
}

export function peakToDb(p: number): number {
  if (! Number.isFinite(p) || p <= DISPLAY_AMP_FLOOR) {
    return DISPLAY_DB_FLOOR;
  }
  return Math.max(DISPLAY_DB_FLOOR, 20 * Math.log10(p));
}
