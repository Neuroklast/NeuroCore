export const PLOT_N = 160;
export const PHASE_STEP = 0.12;

export const DEMO_EXPR: Record<string, string> = {
  sin: "sin(x * 3.14159)",
  cos: "cos(x * 3.14159)",
  tan: "tan(x * 1.2)",
  tanh: "tanh(x * 2.5)",
  sqrt: "sqrt(abs(x))",
  abs: "abs(x)",
  sign: "sign(x)",
  exp: "exp(x) - 1",
  log: "log(abs(x) + 0.05)",
  log10: "log10(abs(x) + 0.05)",
  log2: "log2(abs(x) + 0.05)",
  floor: "floor(x * 4) / 4",
  ceil: "ceil(x * 4) / 4",
  round: "round(x * 4) / 4",
  pow: "pow(abs(x), 2.0) * sign(x)",
  min: "min(x, 0.4)",
  max: "max(x, -0.4)",
  fmod: "fmod(x * 3, 0.6)",
  mod: "mod(x * 3, 0.6)",
  clamp: "clamp(x * 1.8, -0.6, 0.6)",
  atan: "atan(x * 4) / 1.5708",
  asinh: "asinh(x * 3) / asinh(3)",
  lerp: "lerp(x, softclip(x, 3), 0.55)",
  map: "map(x, -1, 1, -0.5, 0.5)",
  step: "x * step(0.25, abs(x))",
  smoothstep: "smoothstep(-0.6, 0.6, x) * 2 - 1",
  noise: "x + noise(x * 25) * 0.2",
  softclip: "softclip(x, 2.5)",
  hardclip: "hardclip(softclip(x, 1.5), 0.55)",
  tube: "tube(x, 2.2)",
  diode: "diode(x, 2.0)",
  fold: "fold(x * 2.5, -0.7, 0.7)",
  wrap: "wrap(x * 2.2, -1, 1)",
  bitcrush: "bitcrush(softclip(x, 1.2), 5)",
  quantize: "quantize(x, 8)",
};

export type PlotKind = "transfer" | "ott" | "widen" | "octaver" | "vocoder";

export function kindForName(name: string): PlotKind {
  const n = name.trim().toLowerCase();
  if (n === "ott" || n.includes("ott")) return "ott";
  if (n === "widen" || n.includes("widen") || n.includes("haas") || n.includes("stereo width")) return "widen";
  if (n === "octaver" || n.includes("octaver") || n === "octave") return "octaver";
  if (n === "vocoder" || n.includes("vocoder")) return "vocoder";
  return "transfer";
}

export function categoryForName(name: string): string {
  const n = name.trim().toLowerCase();
  if (n === "tube" || n === "diode" || n === "tanh" || n === "softclip" || n === "hardclip" || n.includes("clip")) {
    return "Drive";
  }
  if (n === "fold" || n === "wrap" || n === "bitcrush" || n === "quantize") {
    return "Crush";
  }
  if (n.startsWith("param") || n.includes("ott") || n.includes("widen") || n.includes("stereo")
    || n.includes("vocoder") || n.includes("octaver") || n.includes("delay") || n.includes("reverb")
    || n.includes("comp") || n.includes("gate") || n.includes("limit") || n.includes("filter")
    || n.includes("eq") || n.includes("xover") || n.includes("ir") || n.includes("blend")) {
    return "Blocks";
  }
  return "Core";
}

export function resolvePlotExpression(functionName: string, example = ""): string {
  const name = functionName.trim().toLowerCase();
  for (const [key, expr] of Object.entries(DEMO_EXPR)) {
    if (name === key || name.startsWith(`${key} `)) {
      return expr;
    }
  }
  if (name.includes("lerp") && name.includes("blend")) return "lerp(x, tube(x, 2), 0.5)";
  if (name.includes("clip") && name.includes("lpf")) return "hardclip(softclip(x, 2), 0.6)";
  if (name.startsWith("param")) return "x * 0.65";

  let ex = example.trim().split("\n")[0] ?? "";
  if (ex.includes(":") && ! ex.toLowerCase().startsWith("y")) {
    return "softclip(x, 1.5)";
  }
  const eq = ex.indexOf("=");
  if (eq >= 0) {
    ex = ex.slice(eq + 1).trim();
  }
  const swap: Record<string, string> = { a: "2.0", b: "0.5", c: "0.7", d: "0.3", e: "0.6", f: "1.2", y: "x", t: "x", sr: "44100" };
  for (const [id, with_] of Object.entries(swap)) {
    ex = ex.replace(new RegExp(`(?<![A-Za-z0-9_])${id}(?![A-Za-z0-9_])`, "g"), with_);
  }
  return ex || "x";
}

function fns(x: number) {
  const sign = (v: number) => (v < 0 ? -1 : v > 0 ? 1 : 0);
  const clamp = (v: number, lo: number, hi: number) => Math.min(hi, Math.max(lo, v));
  const softclip = (v: number, d = 2.5) => Math.tanh(v * d);
  const hardclip = (v: number, t = 0.55) => clamp(v, -t, t);
  const tube = (v: number, d = 2.2) => Math.tanh(v * d) * (1 + 0.15 * v * v);
  const diode = (v: number, d = 2) => (v < 0 ? 0.15 * Math.tanh(v * d) : Math.tanh(v * d));
  const fold = (v: number, lo = -0.7, hi = 0.7) => {
    let y = v;
    for (let i = 0; i < 6; i += 1) {
      if (y > hi) y = 2 * hi - y;
      else if (y < lo) y = 2 * lo - y;
      else break;
    }
    return y;
  };
  const wrap = (v: number, lo = -1, hi = 1) => {
    const s = hi - lo;
    return ((((v - lo) % s) + s) % s) + lo;
  };
  const lerp = (a: number, b: number, t: number) => a + (b - a) * t;
  const map = (v: number, a0: number, a1: number, b0: number, b1: number) => b0 + ((v - a0) / (a1 - a0 || 1)) * (b1 - b0);
  const step = (edge: number, v: number) => (v >= edge ? 1 : 0);
  const smoothstep = (e0: number, e1: number, v: number) => {
    const t = clamp((v - e0) / (e1 - e0 || 1), 0, 1);
    return t * t * (3 - 2 * t);
  };
  const noise = (v: number) => {
    const s = Math.sin(v * 12.9898) * 43758.5453;
    return (s - Math.floor(s)) * 2 - 1;
  };
  const quantize = (v: number, bits = 8) => {
    const steps = Math.max(2, 2 ** Math.round(bits) - 1);
    return Math.round(clamp(v, -1, 1) * steps) / steps;
  };
  const bitcrush = (v: number, bits = 5) => quantize(v, bits);
  const fmod = (a: number, b: number) => a - Math.floor(a / (b || 1)) * (b || 1);
  return {
    x,
    sin: Math.sin, cos: Math.cos, tan: Math.tan, tanh: Math.tanh,
    sqrt: Math.sqrt, abs: Math.abs, sign, exp: Math.exp,
    log: Math.log, log10: Math.log10, log2: Math.log2,
    floor: Math.floor, ceil: Math.ceil, round: Math.round,
    pow: Math.pow, min: Math.min, max: Math.max, atan: Math.atan,
    asinh: Math.asinh, fmod, mod: fmod, clamp, lerp, map, step, smoothstep,
    noise, softclip, hardclip, tube, diode, fold, wrap, bitcrush, quantize,
  };
}

export function evalExpr(expr: string, x: number): number {
  const env = fns(x);
  const names = Object.keys(env);
  const vals = names.map((k) => (env as Record<string, unknown>)[k]);
  try {
    const y = new Function(...names, `"use strict"; return (${expr});`)(...vals);
    return Number.isFinite(y) ? Math.min(2, Math.max(-2, y as number)) : 0;
  } catch {
    return 0;
  }
}

export function buildTraces(expr: string, phase: number, n = PLOT_N): { inn: Float32Array; out: Float32Array; ok: boolean } {
  const inn = new Float32Array(n);
  const out = new Float32Array(n);
  let ok = true;
  try {
    new Function("x", `"use strict"; return (${expr});`);
  } catch {
    ok = false;
  }
  for (let i = 0; i < n; i += 1) {
    const t = phase + Math.PI * 2 * (i / (n - 1));
    const xin = Math.sin(t);
    inn[i] = xin;
    out[i] = ok ? evalExpr(expr, xin) : 0;
  }
  return { inn, out, ok };
}

export function plotCaption(kind: PlotKind): string {
  if (kind === "ott") return "OTT: three bands, each with its own up/down curve";
  if (kind === "widen") return "Widen: left dry, right delayed / wider (not OTT)";
  if (kind === "octaver") return "Octaver: dry + sub (1/2) + up (2x)";
  if (kind === "vocoder") return "Vocoder: voice envelope on a carrier (not OTT)";
  return "Animated: sine IN (top) vs after function OUT (bottom)";
}
