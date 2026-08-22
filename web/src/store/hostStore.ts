import { create } from "zustand";
import { applyUiPrefs } from "../chrome/persistUi";
import { DEFAULT_THEME, isThemeId, readStoredTheme, THEME_STORAGE_KEY, type ThemeId } from "../theme/theme";

export interface KnobState {
  id: string;
  name: string;
  value: number;
  active: boolean;
  min: number;
  max: number;
  isNote: boolean;
  /** Display unit from chipSpec.ranges (e.g. Hz, %, dB). Not used by DSP. */
  unit?: string;
  /** When set, the knob is an enum detent control (N ticks). */
  enums?: string[];
}

export interface HostState {
  knobs: KnobState[];
  mix: number;
  os: number;
  polisher: number;
  input: number;
  cpu: number;
  mode: string;
  lat: number;
  sr: number;
  buf: number;
  bpm: number;
  tempoSource: string;
  osFactor: number;
  scale: number;
  presetName: string;
  originName: string;
  presetDirty: boolean;
  discardPrompt: boolean;
  frameRate: 0 | 30 | 60;
  pendingPreset: { action: string; name?: string; author?: string; category?: string; tags?: string } | null;
  presets: Array<{
    name: string;
    category: string;
    description: string;
    author: string;
    factory: boolean;
    tags: string[];
  }>;
  licensed: boolean;
  demoRemainSec: number;
  licenseEmail: string;
  systemId: string;
  licenseError: string;
  irSlots: Array<{ slot: string; name: string; loaded: boolean }>;
  overlay: string | null;
  overlayReturn: string | null;
  inspectId: string | null;
  telemetryPath: string;
  bypass: boolean;
  mixHeld: number;
  motion: "full" | "reduced" | "off";
  cables: "dots" | "wave";
  formulaPt: number;
  scopeSource: "in" | "out" | "both";
  scopeDelta: boolean;
  scopeX: "samples" | "time" | "freq";
  scopeY: "linear" | "db";
  scopeGrid: boolean;
  scopeInvertY: boolean;
  /** Host sidechain bus enabled — Sidechain IN chip is visible only when true. */
  sidechainOn: boolean;
  /** Live osc/env tap peaks from the host, keyed by chip id (`env1`, `osc1`). */
  mods: Record<string, number>;
  /** Post-chip peak, keyed by node id (`stage1`, `IN`, `OUT`). */
  clips: Record<string, number>;
  /** Isolated left/right post-chip peaks. Fallback is `clips` when the host omits them. */
  clipsL: Record<string, number>;
  clipsR: Record<string, number>;
  /** Block RMS, same keys. Fallback is the matching peak when the host omits it. */
  clipsRms: Record<string, number>;
  clipsRmsL: Record<string, number>;
  clipsRmsR: Record<string, number>;
  theme: ThemeId;
  knobGestures: Record<string, true>;
  knobMeta: Record<string, Partial<KnobState>>;
  setTheme: (id: ThemeId) => void;
  applyParams: (p: Record<string, unknown>) => void;
  applyHost: (p: Record<string, unknown>) => void;
  applyPresets: (p: Record<string, unknown>) => void;
  applyLicense: (p: Record<string, unknown>) => void;
  applyIr: (p: Record<string, unknown>) => void;
  setOverlay: (name: string | null, inspectId?: string | null) => void;
  setTelemetryPath: (path: string) => void;
  setKnob: (id: string, value: number) => void;
  beginKnobGesture: (id: string) => void;
  endKnobGesture: (id: string) => void;
  patchKnob: (id: string, patch: Partial<KnobState>) => void;
  markDirty: () => void;
  activateKnob: (id: string, patch: Partial<KnobState> & { active: true }) => void;
  setMix: (value: number) => void;
  toggleBypass: () => number;
  setOs: (index: number) => void;
  setPolisher: (index: number) => void;
  setInput: (index: number) => void;
}

function aliasIoPeak(next: Record<string, number>, id: string, peak: number): void {
  next[id] = peak;
  if (id === "__out__") {
    next.OUT = peak;
  }
  if (id === "OUT") {
    next.__out__ = peak;
  }
  if (id === "__in__") {
    next.IN = peak;
  }
  if (id === "IN") {
    next.__in__ = peak;
  }
}

function finiteOr(raw: unknown, fallback: number): number {
  const n = Number(raw);
  return Number.isFinite(n) ? n : fallback;
}

export function parseClipPeaks(rows: Array<Record<string, unknown>>): {
  peak: Record<string, number>;
  peakL: Record<string, number>;
  peakR: Record<string, number>;
  rms: Record<string, number>;
  rmsL: Record<string, number>;
  rmsR: Record<string, number>;
} {
  const peak: Record<string, number> = {};
  const peakL: Record<string, number> = {};
  const peakR: Record<string, number> = {};
  const rms: Record<string, number> = {};
  const rmsL: Record<string, number> = {};
  const rmsR: Record<string, number> = {};
  for (const raw of rows) {
    const id = String(raw.id ?? "");
    if (! id) {
      continue;
    }
    const p = Number(raw.peak ?? 0);
    const l = raw.peakL != null && Number.isFinite(Number(raw.peakL)) ? Number(raw.peakL) : p;
    const r = raw.peakR != null && Number.isFinite(Number(raw.peakR)) ? Number(raw.peakR) : p;
    const hasRms = raw.rms != null || raw.rmsL != null || raw.rmsR != null;
    const e = hasRms ? finiteOr(raw.rms, Math.max(l, r)) : p;
    const eL = hasRms ? finiteOr(raw.rmsL, e) : l;
    const eR = hasRms ? finiteOr(raw.rmsR, e) : r;
    aliasIoPeak(peak, id, p);
    aliasIoPeak(peakL, id, l);
    aliasIoPeak(peakR, id, r);
    aliasIoPeak(rms, id, e);
    aliasIoPeak(rmsL, id, eL);
    aliasIoPeak(rmsR, id, eR);
  }
  return { peak, peakL, peakR, rms, rmsL, rmsR };
}

export function osFactorFromIndex(index: number): number {
  const i = Math.max(0, Math.min(3, Math.round(index)));
  return ([1, 2, 4, 8] as const)[i] ?? 8;
}

export function osIndexFromFactor(factor: number): number {
  if (factor >= 8) return 3;
  if (factor >= 4) return 2;
  if (factor >= 2) return 1;
  return 0;
}

function mergeParamsKnobs(
  current: KnobState[],
  incoming: KnobState[],
  gestures: Record<string, true>,
  meta: Record<string, Partial<KnobState>>,
): KnobState[] {
  const live = new Map(current.map((k) => [k.id, k]));
  return incoming.map((k) => {
    const cur = live.get(k.id);
    let next = k;
    if (cur && (! k.enums || k.enums.length === 0) && cur.enums && cur.enums.length > 0) {
      next = { ...next, enums: cur.enums };
    }
    if (gestures[k.id] && cur) {
      next = { ...next, value: cur.value };
    }
    const lock = meta[k.id];
    if (lock) {
      next = { ...next, ...lock, id: k.id, value: next.value };
    }
    return next;
  });
}

function asKnobs(raw: unknown): KnobState[] {
  if (! Array.isArray(raw)) {
    return [];
  }
  return raw.map((k) => {
    const o = k as Record<string, unknown>;
    const enums = Array.isArray(o.enums) ? o.enums.map(String) : undefined;
    return {
      id: String(o.id ?? ""),
      name: String(o.name ?? ""),
      value: Number(o.value ?? 0),
      active: Boolean(o.active),
      min: Number(o.min ?? 0),
      max: Number(o.max ?? 1),
      isNote: Boolean(o.isNote),
      unit: o.unit != null ? String(o.unit) : undefined,
      enums: enums && enums.length > 0 ? enums : undefined,
    };
  });
}

export const useHostStore = create<HostState>((set) => ({
  knobs: [
    { id: "a", name: "Rate", value: 1, active: true, min: 0.05, max: 6, isNote: false },
    { id: "b", name: "Depth", value: 0.304, active: true, min: 200, max: 2500, isNote: false },
    { id: "c", name: "Center", value: 0.222, active: true, min: 400, max: 4000, isNote: false },
    { id: "d", name: "Drive", value: 0.4, active: true, min: 0.8, max: 1.6, isNote: false },
    { id: "e", name: "Mix", value: 0.643, active: true, min: 0.25, max: 0.95, isNote: false },
    { id: "f", name: "Resonance", value: 0.4, active: true, min: 0.35, max: 1.1, isNote: false },
  ],
  mix: 1,
  os: 2,
  polisher: 0,
  input: 1,
  cpu: 0,
  mode: "STUDIO",
  lat: 0,
  sr: 0,
  buf: 0,
  bpm: 120,
  tempoSource: "HOST",
  osFactor: 4,
  scale: 100,
  presetName: "",
  originName: "",
  presetDirty: false,
  discardPrompt: (() => {
    try {
      return localStorage.getItem("nk-discard-prompt") !== "0";
    } catch {
      return true;
    }
  })(),
  frameRate: (() => {
    try {
      const n = Number(localStorage.getItem("nk-fps") ?? "0");
      return n === 30 || n === 60 ? n : 0;
    } catch {
      return 0;
    }
  })(),
  pendingPreset: null,
  presets: [],
  licensed: true,
  demoRemainSec: 0,
  licenseEmail: "",
  systemId: "",
  licenseError: "",
  irSlots: [],
  overlay: null,
  overlayReturn: null,
  inspectId: null,
  telemetryPath: "",
  bypass: false,
  mixHeld: 1,
  motion: "full",
  cables: "wave",
  formulaPt: 18,
  scopeSource: "both",
  scopeDelta: false,
  scopeX: "samples",
  scopeY: "linear",
  scopeGrid: true,
  scopeInvertY: false,
  sidechainOn: false,
  mods: {},
  clips: {},
  clipsL: {},
  clipsR: {},
  clipsRms: {},
  clipsRmsL: {},
  clipsRmsR: {},
  theme: (() => {
    try {
      if (typeof localStorage === "undefined" || typeof localStorage.getItem !== "function") {
        return DEFAULT_THEME;
      }
      return readStoredTheme(localStorage.getItem(THEME_STORAGE_KEY));
    } catch {
      return DEFAULT_THEME;
    }
  })(),
  knobGestures: {} as Record<string, true>,
  knobMeta: {} as Record<string, Partial<KnobState>>,

  applyParams: (p) => set((s) => ({
    knobs: Array.isArray(p.knobs) ? mergeParamsKnobs(s.knobs, asKnobs(p.knobs), s.knobGestures, s.knobMeta) : s.knobs,
    mix: p.mix != null ? Number(p.mix) : s.mix,
    os: p.os != null ? Number(p.os) : s.os,
    osFactor: p.os != null ? osFactorFromIndex(Number(p.os)) : s.osFactor,
    polisher: p.polisher != null ? Math.max(0, Math.min(1, Number(p.polisher))) : s.polisher,
    input: p.input != null ? Number(p.input) : s.input,
    bypass: p.bypass != null ? Boolean(p.bypass) : s.bypass,
  })),
  applyHost: (p) => set((s) => {
    const prefs = applyUiPrefs(p, {
      motion: s.motion,
      cables: s.cables,
      theme: s.theme,
      frameRate: s.frameRate,
      discardPrompt: s.discardPrompt,
    });
    const parsedClips = Array.isArray(p.clips)
      ? parseClipPeaks(p.clips as Array<Record<string, unknown>>)
      : null;
    return {
    cpu: Number(p.cpu ?? 0),
    mode: String(p.mode ?? "STUDIO"),
    lat: Number(p.lat ?? 0),
    sr: Number(p.sr ?? 0),
    buf: Number(p.buf ?? 0),
    bpm: Number(p.bpm ?? 120),
    tempoSource: String(p.tempoSource ?? "HOST"),
    osFactor: p.os != null ? Number(p.os) : s.osFactor,
    os: p.os != null ? osIndexFromFactor(Number(p.os)) : s.os,
    scale: Number(p.scale ?? 100),
    motion: prefs.motion ?? s.motion,
    cables: prefs.cables ?? s.cables,
    theme: prefs.theme ?? s.theme,
    frameRate: prefs.frameRate ?? s.frameRate,
    discardPrompt: prefs.discardPrompt ?? s.discardPrompt,
    sidechainOn: p.sidechainOn != null ? Boolean(p.sidechainOn) : s.sidechainOn,
    mods: Array.isArray(p.mods)
      ? Object.fromEntries(
        (p.mods as Array<Record<string, unknown>>).map((m) => [
          String(m.id ?? ""),
          Number(m.value ?? 0),
        ] as [string, number]).filter(([id]) => id.length > 0),
      )
      : s.mods,
    clips: parsedClips ? parsedClips.peak : s.clips,
    clipsL: parsedClips ? parsedClips.peakL : s.clipsL,
    clipsR: parsedClips ? parsedClips.peakR : s.clipsR,
    clipsRms: parsedClips ? parsedClips.rms : s.clipsRms,
    clipsRmsL: parsedClips ? parsedClips.rmsL : s.clipsRmsL,
    clipsRmsR: parsedClips ? parsedClips.rmsR : s.clipsRmsR,
  };
  }),
  applyPresets: (p) => set((s) => {
    const name = String(p.name ?? s.presetName);
    const nameChanged = name !== s.presetName;
    return {
    presetName: name,
    originName: nameChanged ? name : (s.originName || name),
    presetDirty: nameChanged ? false : s.presetDirty,
    pendingPreset: nameChanged ? null : s.pendingPreset,
    knobMeta: nameChanged ? {} : s.knobMeta,
    presets: Array.isArray(p.list)
      ? (p.list as Array<Record<string, unknown>>).map((e) => ({
          name: String(e.name ?? ""),
          category: String(e.category ?? ""),
          description: String(e.description ?? ""),
          author: String(e.author ?? "Neuroklast"),
          factory: e.factory !== false,
          tags: Array.isArray(e.tags) ? e.tags.map(String) : [],
        }))
      : s.presets,
  };
  }),
  applyLicense: (p) => set({
    licensed: Boolean(p.licensed),
    demoRemainSec: Number(p.demoRemainSec ?? 0),
    licenseEmail: String(p.email ?? ""),
    systemId: String(p.systemId ?? ""),
    licenseError: String(p.error ?? ""),
  }),
  applyIr: (p) => set({
    irSlots: Array.isArray(p.slots)
      ? (p.slots as Array<Record<string, unknown>>).map((s) => ({
          slot: String(s.slot ?? ""),
          name: String(s.name ?? ""),
          loaded: Boolean(s.loaded),
        }))
      : [],
  }),
  setOverlay: (name, inspectId) => set({ overlay: name, overlayReturn: null, inspectId: inspectId ?? null }),
  setTelemetryPath: (telemetryPath: string) => set({ telemetryPath }),
  setKnob: (id, value) => set((s) => ({
    knobs: s.knobs.map((k) => (k.id === id ? { ...k, value: Math.max(0, Math.min(1, value)) } : k)),
    presetDirty: true,
  })),
  markDirty: () => set({ presetDirty: true }),
  beginKnobGesture: (id) => set((s) => (
    s.knobGestures[id] ? s : { knobGestures: { ...s.knobGestures, [id]: true } }
  )),
  endKnobGesture: (id) => set((s) => {
    if (! s.knobGestures[id]) {
      return s;
    }
    const knobGestures = { ...s.knobGestures };
    delete knobGestures[id];
    return { knobGestures };
  }),
  patchKnob: (id, patch) => set((s) => ({
    knobs: s.knobs.map((k) => (k.id === id ? { ...k, ...patch, id: k.id } : k)),
    knobMeta: { ...s.knobMeta, [id]: { ...s.knobMeta[id], ...patch } },
    presetDirty: true,
  })),
  activateKnob: (id, patch) => set((s) => ({
    knobs: s.knobs.map((k) => {
      if (k.id !== id) {
        return k;
      }
      const value = patch.value != null
        ? Math.max(0, Math.min(1, patch.value))
        : k.value;
      return {
        ...k,
        ...patch,
        id: k.id,
        active: true,
        value,
        enums: patch.enums && patch.enums.length > 0 ? patch.enums : undefined,
      };
    }),
  })),
  setMix: (value) => set(() => {
    const mix = Math.max(0, Math.min(1, value));
    if (mix > 1e-5) {
      return { mix, mixHeld: mix, bypass: false, presetDirty: true };
    }
    return { mix, bypass: true, presetDirty: true };
  }),
  toggleBypass: () => {
    let next = 0;
    set((s) => {
      if (s.mix <= 1e-5) {
        next = s.mixHeld;
        return { mix: next, bypass: false };
      }
      next = 0;
      return { mixHeld: s.mix, mix: 0, bypass: true };
    });
    return next;
  },
  setOs: (index) => {
    const os = Math.max(0, Math.min(3, Math.round(index)));
    set({ os, osFactor: osFactorFromIndex(os) });
  },
  setPolisher: (index) => set({ polisher: Math.max(0, Math.min(1, Math.round(index))) }),
  setInput: (index) => set({ input: Math.max(0, Math.min(2, Math.round(index))) }),
  setTheme: (id) => {
    const theme = isThemeId(id) ? id : DEFAULT_THEME;
    try {
      localStorage.setItem(THEME_STORAGE_KEY, theme);
    } catch {
      /* private mode */
    }
    set({ theme });
  },
}));
