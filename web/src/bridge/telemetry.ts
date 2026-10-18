export const TELEMETRY_MAGIC = 0x4E4B544D;
export const TELEMETRY_VERSION_MIN = 1;
export const TELEMETRY_VERSION_MAX = 3;
export const TELEMETRY_TAIL = 8;
export const TELEMETRY_ONSET_MAX = 16;
export const TELEMETRY_ONSET_HEADER = 4;
export const TELEMETRY_ONSET_BYTES = 12;
export const SCOPE_N = 2048;
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
  onsetCount: number;
  onsetAmp: number;
  seq: number;
  samplePos: number;
  onsets: { samplePos: number; amp: number; balance: number }[];
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
    onsetCount: 0,
    onsetAmp: 0,
    seq: 0,
    samplePos: 0,
    onsets: [],
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
  const version = view.getUint16(4, true);
  const scopeN = view.getUint16(28, true);
  const gonioN = view.getUint16(30, true);
  const body = 32 + (scopeN * 2 + gonioN * 2) * 4;
  let requiredBytes = version >= 2 ? body + TELEMETRY_TAIL : body;
  if (version >= 3) {
    requiredBytes += TELEMETRY_ONSET_HEADER;
  }
  if (version < TELEMETRY_VERSION_MIN || version > TELEMETRY_VERSION_MAX || ab.byteLength < requiredBytes) {
    return false;
  }
  const inPeak = view.getFloat32(8, true);
  const outPeak = view.getFloat32(12, true);
  const inRms = view.getFloat32(16, true);
  const outRms = view.getFloat32(20, true);
  const cpu01 = view.getFloat32(24, true);
  if (
    ! Number.isFinite(inPeak) || inPeak < 0
    || ! Number.isFinite(outPeak) || outPeak < 0
    || ! Number.isFinite(inRms) || inRms < 0
    || ! Number.isFinite(outRms) || outRms < 0
    || ! Number.isFinite(cpu01) || cpu01 < 0
  ) {
    return false;
  }
  dest.inPeak = inPeak;
  dest.outPeak = outPeak;
  dest.inRms = inRms;
  dest.outRms = outRms;
  dest.cpu01 = cpu01;
  const packed = view.getUint16(6, true);
  dest.onsetCount = (packed >> 8) & 0xff;
  dest.onsetAmp = (packed & 0xff) / 255;
  dest.seq = 0;
  dest.samplePos = 0;
  dest.onsets = [];
  if (version >= 2) {
    dest.seq = view.getUint32(body, true);
    dest.samplePos = view.getUint32(body + 4, true);
  }
  if (version >= 3) {
    const list = body + TELEMETRY_TAIL;
    const nOn = view.getUint16(list, true);
    if (nOn > TELEMETRY_ONSET_MAX || ab.byteLength < list + TELEMETRY_ONSET_HEADER + nOn * TELEMETRY_ONSET_BYTES) {
      return false;
    }
    for (let i = 0; i < nOn; i += 1) {
      const at = list + TELEMETRY_ONSET_HEADER + i * TELEMETRY_ONSET_BYTES;
      const amp = view.getFloat32(at + 4, true);
      const bal = view.getFloat32(at + 8, true);
      dest.onsets.push({
        samplePos: view.getUint32(at, true),
        amp: Number.isFinite(amp) ? Math.max(0, Math.min(1, amp)) : 0,
        balance: Number.isFinite(bal) ? Math.max(-1, Math.min(1, bal)) : 0,
      });
    }
  }
  dest.scopeN = Math.min(scopeN, dest.scopeIn.length);
  dest.gonioN = Math.min(gonioN, dest.gonioX.length);
  const f32 = new Float32Array(ab, 32, scopeN * 2 + gonioN * 2);
  const copy = (target: Float32Array, offset: number, length: number) => {
    if (length === target.length) {
      target.set(f32.subarray(offset, offset + length));
      for (let i = 0; i < length; i += 1) {
        if (! Number.isFinite(target[i]!)) {
          target[i] = 0;
        }
      }
      return;
    }
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

/** Native gonio buffers are raw L/R. Pearson correlation of those channels. */
export function gonioCorr(left: ArrayLike<number>, right: ArrayLike<number>): number {
  const n = Math.min(left.length, right.length);
  let accLr = 0;
  let accL2 = 0;
  let accR2 = 0;
  for (let i = 0; i < n; i += 1) {
    const l = left[i] ?? 0;
    const r = right[i] ?? 0;
    accLr += l * r;
    accL2 += l * l;
    accR2 += r * r;
  }
  const den = Math.sqrt(accL2 * accR2);
  if (! (den > 1e-12)) {
    return 0;
  }
  const corr = accLr / den;
  return corr < -1 ? -1 : corr > 1 ? 1 : corr;
}

export function peakToDb(p: number): number {
  if (! Number.isFinite(p) || p <= DISPLAY_AMP_FLOOR) {
    return DISPLAY_DB_FLOOR;
  }
  return Math.max(DISPLAY_DB_FLOOR, 20 * Math.log10(p));
}
