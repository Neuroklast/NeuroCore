import { beforeEach, describe, expect, it } from "vitest";
import { useHostStore } from "./hostStore";

describe("hostStore", () => {
  beforeEach(() => {
    useHostStore.setState({
      cpu: 0,
      mode: "STUDIO",
      knobs: [],
      overlay: null,
    });
  });

  it("clamps host cpu as given 0-100 from the bridge", () => {
    useHostStore.getState().applyHost({ cpu: 100, mode: "SAFE", os: 4 });
    expect(useHostStore.getState().cpu).toBe(100);
    expect(useHostStore.getState().mode).toBe("SAFE");
    expect(useHostStore.getState().osFactor).toBe(4);
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
