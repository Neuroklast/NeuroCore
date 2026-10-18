/** Deterministic fixtures with known analytic properties. Never Math.random. */

export function silence(n: number): Float32Array {
  return new Float32Array(Math.max(0, n));
}

export function tone(hz: number, sr: number, n: number, amp = 1): Float32Array {
  const out = new Float32Array(Math.max(0, n));
  const rate = sr > 0 ? sr : 48000;
  for (let i = 0; i < out.length; i += 1) {
    out[i] = amp * Math.sin((2 * Math.PI * hz * i) / rate);
  }
  return out;
}

export function impulse(n: number, at = -1, amp = 1): Float32Array {
  const out = new Float32Array(Math.max(0, n));
  if (out.length < 1) {
    return out;
  }
  const i = at >= 0 ? Math.min(out.length - 1, at) : out.length >> 1;
  out[i] = amp;
  return out;
}

/** Two clicks `gap` samples apart. */
export function twoImpulses(n: number, gap: number, amp = 1): Float32Array {
  const out = new Float32Array(Math.max(0, n));
  const a = Math.max(1, Math.floor(out.length * 0.3));
  const b = Math.min(out.length - 1, a + Math.max(1, gap));
  out[a] = amp;
  out[b] = amp;
  return out;
}

export function kickDecay(sr: number, n: number, hz = 55, tau = 0.08, amp = 1): Float32Array {
  const out = new Float32Array(Math.max(0, n));
  const rate = sr > 0 ? sr : 48000;
  const decay = Math.max(1e-4, tau);
  for (let i = 0; i < out.length; i += 1) {
    const t = i / rate;
    out[i] = amp * Math.exp(-t / decay) * Math.sin((2 * Math.PI * hz * i) / rate);
  }
  return out;
}

export function periodicKick(sr: number, n: number, bpm = 120, hz = 55): Float32Array {
  const out = new Float32Array(Math.max(0, n));
  const rate = sr > 0 ? sr : 48000;
  const period = Math.max(1, Math.round((60 / bpm) * rate));
  const length = Math.min(period, Math.round(0.08 * rate));
  for (let start = 0; start < out.length; start += period) {
    const hit = kickDecay(rate, length, hz, 0.05, 1);
    out.set(hit.subarray(0, Math.min(hit.length, out.length - start)), start);
  }
  return out;
}

export function noise(n: number, amp = 0.2, seed = 1): Float32Array {
  const out = new Float32Array(Math.max(0, n));
  let s = seed >>> 0;
  for (let i = 0; i < out.length; i += 1) {
    s = (s * 1664525 + 1013904223) >>> 0;
    out[i] = amp * ((s / 4294967296) * 2 - 1);
  }
  return out;
}

export function stereoPair(
  left: ArrayLike<number>,
  right: ArrayLike<number>,
): { l: Float32Array; r: Float32Array } {
  const n = Math.min(left.length, right.length);
  const l = new Float32Array(n);
  const r = new Float32Array(n);
  for (let i = 0; i < n; i += 1) {
    l[i] = left[i] ?? 0;
    r[i] = right[i] ?? 0;
  }
  return { l, r };
}

/** Gonio of L/R: x = 0.5(L−R), y = −0.5(L+R). */
export function gonioFromLr(l: ArrayLike<number>, r: ArrayLike<number>, n: number): {
  x: Float32Array;
  y: Float32Array;
} {
  const count = Math.max(0, n);
  const x = new Float32Array(count);
  const y = new Float32Array(count);
  const ln = l.length;
  const rn = r.length;
  for (let i = 0; i < count; i += 1) {
    const a = ln > 0 ? l[i % ln] ?? 0 : 0;
    const b = rn > 0 ? r[i % rn] ?? 0 : 0;
    x[i] = 0.5 * (a - b);
    y[i] = -0.5 * (a + b);
  }
  return { x, y };
}
