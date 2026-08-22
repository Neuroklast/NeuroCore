import { beforeEach, describe, expect, it } from "vitest";
import { useHostStore } from "./hostStore";

describe("hostStore", () => {
  beforeEach(() => {
    useHostStore.setState({
      cpu: 0,
      mode: "STUDIO",
      knobs: [],
      overlay: null,
      knobGestures: {},
    });
  });

  it("initial os/osFactor/polisher match APVTS defaults (Issue 5: no cross-instance flash)", () => {
    // The store initialises with values that match the plugin's APVTS defaults:
    //   kDefaultOversamplingIndex = 2  →  4×  (index 2, factor 4)
    //   polisherMode default          =  0   (Soft Clip Off)
    // If these were wrong (e.g. 3/8/2) a new instance would briefly display the
    // previous instance's settings before the bridge pushHost() update arrives.
    const state = useHostStore.getInitialState();
    expect(state.os).toBe(2);
    expect(state.osFactor).toBe(4);
    expect(state.polisher).toBe(0);
  });

  it("clamps host cpu as given 0-100 from the bridge", () => {
    useHostStore.getState().applyHost({ cpu: 100, mode: "SAFE", os: 4 });
    expect(useHostStore.getState().cpu).toBe(100);
    expect(useHostStore.getState().mode).toBe("SAFE");
    expect(useHostStore.getState().osFactor).toBe(4);
  });

  it("stores per-chip clip peaks from the host snapshot", () => {
    useHostStore.getState().applyHost({
      clips: [{ id: "stage1", peak: 1.2 }, { id: "__out__", peak: 0.4 }],
    });
    expect(useHostStore.getState().clips.stage1).toBeCloseTo(1.2);
    expect(useHostStore.getState().clips.OUT).toBeCloseTo(0.4);
    expect(useHostStore.getState().clips.__out__).toBeCloseTo(0.4);
    expect(useHostStore.getState().clipsL.stage1).toBeCloseTo(1.2);
    expect(useHostStore.getState().clipsR.stage1).toBeCloseTo(1.2);
  });

  it("keeps L and R clip peaks isolated when the host sends both", () => {
    useHostStore.getState().applyHost({
      clips: [{ id: "stage1", peak: 0.8, peakL: 0.8, peakR: 0.02 }],
    });
    expect(useHostStore.getState().clips.stage1).toBeCloseTo(0.8);
    expect(useHostStore.getState().clipsL.stage1).toBeCloseTo(0.8);
    expect(useHostStore.getState().clipsR.stage1).toBeCloseTo(0.02);
  });

  it("stores per-lane rms from the host tap tuple, not a copy of peak", () => {
    useHostStore.getState().applyHost({
      clips: [{ id: "stage1", peak: 0.8, peakL: 0.8, peakR: 0.02, rms: 0.56, rmsL: 0.56, rmsR: 0.01 }],
    });
    const s = useHostStore.getState();
    expect(s.clipsRms.stage1).toBeCloseTo(0.56);
    expect(s.clipsRmsL.stage1).toBeCloseTo(0.56);
    expect(s.clipsRmsR.stage1).toBeCloseTo(0.01);
    expect(s.clipsRmsL.stage1).toBeLessThan(s.clipsL.stage1);
  });

  it("falls back to peak when the host omits rms so old snapshots still stream", () => {
    useHostStore.getState().applyHost({
      clips: [{ id: "stage1", peak: 0.5, peakL: 0.5, peakR: 0.1 }],
    });
    const s = useHostStore.getState();
    expect(s.clipsRms.stage1).toBeCloseTo(0.5);
    expect(s.clipsRmsL.stage1).toBeCloseTo(0.5);
    expect(s.clipsRmsR.stage1).toBeCloseTo(0.1);
  });

  it("stores env tap peaks from host.mods", () => {
    useHostStore.getState().applyHost({
      mods: [{ id: "env1", value: 0.62 }, { id: "osc1", value: 0.4 }],
    });
    expect(useHostStore.getState().mods.env1).toBeCloseTo(0.62);
    expect(useHostStore.getState().mods.osc1).toBeCloseTo(0.4);
  });

  it("stores sidechainOn from the host snapshot", () => {
    useHostStore.getState().applyHost({ sidechainOn: true });
    expect(useHostStore.getState().sidechainOn).toBe(true);
    useHostStore.getState().applyHost({ sidechainOn: false });
    expect(useHostStore.getState().sidechainOn).toBe(false);
  });

  it("marks dirty until a preset load clears it", () => {
    useHostStore.setState({ presetDirty: false, originName: "", discardPrompt: true });
    useHostStore.getState().markDirty();
    expect(useHostStore.getState().presetDirty).toBe(true);
    useHostStore.getState().applyPresets({ name: "Airy Clean", list: [] });
    expect(useHostStore.getState().presetDirty).toBe(false);
    expect(useHostStore.getState().originName).toBe("Airy Clean");
  });

  it("stores knob letters and mix", () => {
    useHostStore.getState().applyParams({
      knobs: [{ id: "a", name: "Drive", value: 0.4, active: true, min: 0, max: 2, isNote: false }],
      mix: 0.8,
    });
    expect(useHostStore.getState().knobs[0]?.id).toBe("a");
    expect(useHostStore.getState().mix).toBe(0.8);
  });

  it("restores held mix after bypass", () => {
    useHostStore.getState().setMix(0.42);
    expect(useHostStore.getState().toggleBypass()).toBe(0);
    expect(useHostStore.getState().mix).toBe(0);
    expect(useHostStore.getState().bypass).toBe(true);
    expect(useHostStore.getState().toggleBypass()).toBeCloseTo(0.42);
    expect(useHostStore.getState().mix).toBeCloseTo(0.42);
    expect(useHostStore.getState().bypass).toBe(false);
  });

  it("writes mix from the slider without resetting os", () => {
    useHostStore.setState({ mix: 1, os: 3, polisher: 1, input: 1 });
    useHostStore.getState().setMix(0.35);
    expect(useHostStore.getState().mix).toBe(0.35);
    useHostStore.getState().applyParams({ knobs: [], mix: 0.5 });
    expect(useHostStore.getState().mix).toBe(0.5);
    expect(useHostStore.getState().os).toBe(3);
    expect(useHostStore.getState().polisher).toBe(1);
    expect(useHostStore.getState().input).toBe(1);
  });

  it("keeps a local knob gesture while the host echoes params", () => {
    useHostStore.setState({
      knobs: [{ id: "a", name: "Drive", value: 0.4, active: true, min: 0, max: 2, isNote: false }],
    });
    useHostStore.getState().beginKnobGesture("a");
    useHostStore.getState().setKnob("a", 0.7);
    useHostStore.getState().applyParams({
      knobs: [{ id: "a", name: "Drive", value: 0.4, active: true, min: 0, max: 2, isNote: false }],
    });
    expect(useHostStore.getState().knobs[0]?.value).toBeCloseTo(0.7);
    useHostStore.getState().endKnobGesture("a");
    useHostStore.getState().applyParams({
      knobs: [{ id: "a", name: "Drive", value: 0.55, active: true, min: 0, max: 2, isNote: false }],
    });
    expect(useHostStore.getState().knobs[0]?.value).toBeCloseTo(0.55);
  });

  it("keeps note-range and min/max from the knob menu across host echoes", () => {
    useHostStore.setState({
      knobs: [{ id: "d", name: "d", value: 0, active: true, min: 0, max: 1, isNote: false }],
      knobMeta: {},
    });
    useHostStore.getState().patchKnob("d", { isNote: true, name: "Delay" });
    useHostStore.getState().applyParams({
      knobs: [{ id: "d", name: "d", value: 0.2, active: true, min: 0, max: 1, isNote: false }],
    });
    expect(useHostStore.getState().knobs[0]?.isNote).toBe(true);
    expect(useHostStore.getState().knobs[0]?.name).toBe("Delay");
    expect(useHostStore.getState().knobs[0]?.value).toBeCloseTo(0.2);
  });

  it("keeps enum detents when the host params echo omits enums", () => {
    useHostStore.setState({
      knobs: [{
        id: "a",
        name: "Type",
        value: 0,
        active: true,
        min: 0,
        max: 1,
        isNote: false,
        enums: ["lowpass", "highpass", "bandpass"],
      }],
    });
    useHostStore.getState().applyParams({
      knobs: [{ id: "a", name: "Type", value: 0, active: true, min: 0, max: 1, isNote: false }],
    });
    expect(useHostStore.getState().knobs[0]?.enums).toEqual(["lowpass", "highpass", "bandpass"]);
  });

  it("footer OS factor follows the mix dropdown index", () => {
    useHostStore.setState({ os: 3, osFactor: 4 });
    useHostStore.getState().setOs(3);
    expect(useHostStore.getState().os).toBe(3);
    expect(useHostStore.getState().osFactor).toBe(8);
    useHostStore.getState().setOs(2);
    expect(useHostStore.getState().osFactor).toBe(4);
    useHostStore.getState().applyParams({ os: 1 });
    expect(useHostStore.getState().os).toBe(1);
    expect(useHostStore.getState().osFactor).toBe(2);
  });
});
