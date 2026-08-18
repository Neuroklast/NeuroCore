import { create } from "zustand";
import { GONIO_N, SCOPE_N, type TelemetryViews } from "../bridge/telemetry";

interface TelemetryState {
  tick: number;
  inPeak: number;
  outPeak: number;
  inRms: number;
  outRms: number;
  scopeIn: Float32Array;
  scopeOut: Float32Array;
  gonioL: Float32Array;
  gonioR: Float32Array;
  applyViews: (v: TelemetryViews) => void;
  waveFor: (nodeId: string) => Float32Array;
}

export const useTelemetryStore = create<TelemetryState>((set, get) => ({
  tick: 0,
  inPeak: 0,
  outPeak: 0,
  inRms: 0,
  outRms: 0,
  scopeIn: new Float32Array(SCOPE_N),
  scopeOut: new Float32Array(SCOPE_N),
  gonioL: new Float32Array(GONIO_N),
  gonioR: new Float32Array(GONIO_N),
  applyViews: (v) => {
    const s = get();
    s.scopeIn.set(v.scopeIn.subarray(0, s.scopeIn.length));
    s.scopeOut.set(v.scopeOut.subarray(0, s.scopeOut.length));
    s.gonioL.set(v.gonioX.subarray(0, s.gonioL.length));
    s.gonioR.set(v.gonioY.subarray(0, s.gonioR.length));
    set({
      tick: s.tick + 1,
      inPeak: v.inPeak,
      outPeak: v.outPeak,
      inRms: v.inRms,
      outRms: v.outRms,
    });
  },
  waveFor: (nodeId) => {
    const s = get();
    if (nodeId === "IN") {
      return s.scopeIn;
    }
    return s.scopeOut;
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
