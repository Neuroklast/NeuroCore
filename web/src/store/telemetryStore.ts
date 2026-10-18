import { create } from "zustand";
import { GONIO_N, SCOPE_N, gonioCorr, type TelemetryViews } from "../bridge/telemetry";

const EMPTY_WAVE = new Float32Array(0);

interface TelemetryState {
  tick: number;
  available: boolean;
  inPeak: number;
  outPeak: number;
  inRms: number;
  outRms: number;
  scopeIn: Float32Array;
  scopeOut: Float32Array;
  gonioL: Float32Array;
  gonioR: Float32Array;
  onsetCount: number;
  onsetAmp: number;
  seq: number;
  samplePos: number;
  corr: number;
  applyViews: (v: TelemetryViews, available?: boolean) => void;
  waveFor: (nodeId: string) => Float32Array;
}

export const useTelemetryStore = create<TelemetryState>((set, get) => ({
  tick: 0,
  available: false,
  inPeak: 0,
  outPeak: 0,
  inRms: 0,
  outRms: 0,
  scopeIn: new Float32Array(SCOPE_N),
  scopeOut: new Float32Array(SCOPE_N),
  gonioL: new Float32Array(GONIO_N),
  gonioR: new Float32Array(GONIO_N),
  onsetCount: 0,
  onsetAmp: 0,
  seq: 0,
  samplePos: 0,
  corr: 0,
  applyViews: (v, available = true) => {
    const s = get();
    if (v.seq > 0 && v.seq === s.seq) {
      return;
    }
    s.scopeIn.set(v.scopeIn.subarray(0, s.scopeIn.length));
    s.scopeOut.set(v.scopeOut.subarray(0, s.scopeOut.length));
    s.gonioL.set(v.gonioX.subarray(0, s.gonioL.length));
    s.gonioR.set(v.gonioY.subarray(0, s.gonioR.length));
    set({
      tick: s.tick + 1,
      available,
      inPeak: v.inPeak,
      outPeak: v.outPeak,
      inRms: v.inRms,
      outRms: v.outRms,
      onsetCount: v.onsetCount,
      onsetAmp: v.onsetAmp,
      seq: v.seq,
      samplePos: v.samplePos,
      corr: gonioCorr(s.gonioL, s.gonioR),
    });
  },
  waveFor: (nodeId) => {
    const s = get();
    const id = nodeId.toUpperCase();
    if (id === "IN") {
      return s.scopeIn;
    }
    if (id === "OUT") {
      return s.scopeOut;
    }
    return EMPTY_WAVE;
  },
}));

export const useBindStore = create<{
  letter: string | null;
  hover: string | null;
  x: number;
  y: number;
  ox: number;
  oy: number;
  start: (letter: string, x: number, y: number) => void;
  move: (x: number, y: number) => void;
  end: () => void;
  setHover: (letter: string | null) => void;
}>((set) => ({
  letter: null,
  hover: null,
  x: 0,
  y: 0,
  ox: 0,
  oy: 0,
  start: (letter, x, y) => set({ letter, x, y, ox: x, oy: y }),
  move: (x, y) => set({ x, y }),
  end: () => set({ letter: null }),
  setHover: (hover) => set({ hover }),
}));
