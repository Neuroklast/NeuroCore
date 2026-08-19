import { create } from "zustand";
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
  irSlots: Array<{ slot: string; name: string; loaded: boolean }>;
  overlay: string | null;
  inspectId: string | null;
  telemetryPath: string;
  bypass: boolean;
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
  theme: ThemeId;
  knobGestures: Record<string, true>;
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
  activateKnob: (id: string, patch: Partial<KnobState> & { active: true }) => void;
  setMix: (value: number) => void;
  setOs: (index: number) => void;
  setPolisher: (index: number) => void;
  setInput: (index: number) => void;
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
): KnobState[] {
  if (Object.keys(gestures).length === 0) {
    return incoming;
  }
  const live = new Map(current.map((k) => [k.id, k]));
  return incoming.map((k) => {
    if (! gestures[k.id]) {
      return k;
    }
    const cur = live.get(k.id);
    return cur ? { ...k, value: cur.value } : k;
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
  presets: [],
  licensed: true,
  demoRemainSec: 0,
  irSlots: [],
  overlay: null,
  inspectId: null,
  telemetryPath: "",
  bypass: false,
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

  applyParams: (p) => set((s) => ({
    knobs: Array.isArray(p.knobs) ? mergeParamsKnobs(s.knobs, asKnobs(p.knobs), s.knobGestures) : s.knobs,
    mix: p.mix != null ? Number(p.mix) : s.mix,
    os: p.os != null ? Number(p.os) : s.os,
    osFactor: p.os != null ? osFactorFromIndex(Number(p.os)) : s.osFactor,
    polisher: p.polisher != null ? Number(p.polisher) : s.polisher,
    input: p.input != null ? Number(p.input) : s.input,
    bypass: p.bypass != null ? Boolean(p.bypass) : s.bypass,
  })),
  applyHost: (p) => set((s) => ({
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
    sidechainOn: p.sidechainOn != null ? Boolean(p.sidechainOn) : s.sidechainOn,
    mods: Array.isArray(p.mods)
      ? Object.fromEntries(
        (p.mods as Array<Record<string, unknown>>).map((m) => [
          String(m.id ?? ""),
          Number(m.value ?? 0),
        ] as [string, number]).filter(([id]) => id.length > 0),
      )
      : s.mods,
  })),
  applyPresets: (p) => set((s) => ({
    presetName: String(p.name ?? s.presetName),
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
  })),
  applyLicense: (p) => set({
    licensed: Boolean(p.licensed),
    demoRemainSec: Number(p.demoRemainSec ?? 0),
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
  setOverlay: (name, inspectId) => set({ overlay: name, inspectId: inspectId ?? null }),
  setTelemetryPath: (telemetryPath: string) => set({ telemetryPath }),
  setKnob: (id, value) => set((s) => ({
    knobs: s.knobs.map((k) => (k.id === id ? { ...k, value: Math.max(0, Math.min(1, value)) } : k)),
  })),
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
  setMix: (value) => set({ mix: Math.max(0, Math.min(1, value)) }),
  setOs: (index) => {
    const os = Math.max(0, Math.min(3, Math.round(index)));
    set({ os, osFactor: osFactorFromIndex(os) });
  },
  setPolisher: (index) => set({ polisher: Math.max(0, Math.min(2, Math.round(index))) }),
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
