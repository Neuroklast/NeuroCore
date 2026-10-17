function clamp01(n: number): number {
  if (! Number.isFinite(n) || n <= 0) {
    return 0;
  }
  return n >= 1 ? 1 : n;
}

export type OnsetState = {
  fast: number;
  slow: number;
  hold: number;
};

export const ONSET_STATIC: OnsetState = { fast: 0, slow: 0, hold: 0 };

function coeff(dt: number, tau: number): number {
  if (! (dt > 0) || ! (tau > 0)) {
    return 1;
  }
  return 1 - Math.exp(-dt / tau);
}

/** Fast/slow envelope flux with refractory. One scan of a block. */
export function onsetScan(
  prev: OnsetState | null,
  samples: ArrayLike<number>,
  sr: number,
  holdSec = 0.045,
): { state: OnsetState; count: number; intensity: number } {
  const n = samples.length;
  const rate = sr > 0 ? sr : 48000;
  const dt = 1 / rate;
  const fr = coeff(dt, 0.05);
  const sa = coeff(dt, 0.025);
  const srls = coeff(dt, 0.22);
  const holdN = Math.max(1, Math.round(holdSec * rate));
  let fast = prev?.fast ?? 0;
  let slow = prev?.slow ?? 0;
  let hold = prev?.hold ?? 0;
  let count = 0;
  let peak = 0;
  for (let i = 0; i < n; i += 1) {
    const x = samples[i] ?? 0;
    const a = Number.isFinite(x) ? Math.abs(x) : 0;
    if (a > fast) {
      fast = a;
    } else {
      fast += (a - fast) * fr;
    }
    slow += (a - slow) * (a > slow ? sa : srls);
    const flux = fast - slow;
    if (hold > 0) {
      hold -= 1;
      continue;
    }
    const gate = 0.055 + 0.42 * slow;
    if (flux > gate && a > 0.12) {
      count += 1;
      if (flux > peak) {
        peak = flux;
      }
      hold = holdN;
    }
  }
  return {
    state: { fast, slow, hold },
    count,
    intensity: count > 0 ? clamp01(peak * 1.8) : 0,
  };
}

export function packOnset(count: number, intensity: number): number {
  const c = Math.max(0, Math.min(255, Math.round(count)));
  const a = Math.max(0, Math.min(255, Math.round(clamp01(intensity) * 255)));
  return (c << 8) | a;
}

export function unpackOnset(u16: number): { count: number; intensity: number } {
  const v = u16 & 0xffff;
  return { count: (v >> 8) & 0xff, intensity: (v & 0xff) / 255 };
}
