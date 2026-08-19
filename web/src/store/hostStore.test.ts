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
    //   polisherMode default          =  0   (None)
    // If these were wrong (e.g. 3/8/2) a new instance would briefly display the
    // previous instance's settings before the bridge pushHost() update arrives.
    const state = useHostStore.getInitialState();
    expect(state.os).toBe(2);
    expect(state.osFactor).toBe(4);
    expect(state.polisher).toBe(0);
  });

    useHostStore.getState().applyHost({ cpu: 100, mode: "SAFE", os: 4 });
    expect(useHostStore.getState().cpu).toBe(100);
    expect(useHostStore.getState().mode).toBe("SAFE");
    expect(useHostStore.getState().osFactor).toBe(4);
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

  it("stores knob letters and mix", () => {
    useHostStore.getState().applyParams({
      knobs: [{ id: "a", name: "Drive", value: 0.4, active: true, min: 0, max: 2, isNote: false }],
      mix: 0.8,
    });
    expect(useHostStore.getState().knobs[0]?.id).toBe("a");
    expect(useHostStore.getState().mix).toBe(0.8);
  });

  it("writes mix from the slider without resetting os", () => {
    useHostStore.setState({ mix: 1, os: 3, polisher: 2, input: 1 });
    useHostStore.getState().setMix(0.35);
    expect(useHostStore.getState().mix).toBe(0.35);
    useHostStore.getState().applyParams({ knobs: [], mix: 0.5 });
    expect(useHostStore.getState().mix).toBe(0.5);
    expect(useHostStore.getState().os).toBe(3);
    expect(useHostStore.getState().polisher).toBe(2);
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
