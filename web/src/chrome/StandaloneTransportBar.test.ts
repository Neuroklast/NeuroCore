import { afterEach, describe, expect, it } from "vitest";
import { createElement } from "react";
import { act, create, type ReactTestRenderer } from "react-test-renderer";
import { StandaloneTransportBar } from "./StandaloneTransportBar";
import { useHostStore } from "../store/hostStore";

const original = useHostStore.getState();
const mounted: ReactTestRenderer[] = [];
function show(standalone: boolean, playing = false, source = "input", ppq = 0) {
  act(() => useHostStore.setState({ standalone, transportPlaying: playing, transportPpq: ppq, auditionSource: source, bpm: 128 }));
  let renderer!: ReactTestRenderer;
  act(() => { renderer = create(createElement(StandaloneTransportBar)); });
  mounted.push(renderer);
  return renderer.root;
}
afterEach(() => {
  act(() => mounted.splice(0).forEach(r => r.unmount()));
  useHostStore.setState(original);
});

describe("standalone transport", () => {
  it("does not expose internal playback, file or tone controls in DAWs", () => {
    expect(show(false).findAllByProps({ "aria-label": "Standalone Transport" })).toHaveLength(0);
  });
  it("defaults to external input with no test generator and stopped transport", () => {
    const root = show(true);
    expect(root.findByProps({ "aria-label": "Stop" })).toBeTruthy();
    expect(root.findByProps({ "aria-label": "Play" }).props["aria-pressed"]).toBe(false);
    expect(root.findByProps({ "aria-label": "Standalone audio source" }).props.value).toBe("input");
    expect(root.findAllByType("button").some(b => b.children.includes("LOAD AUDIO…"))).toBe(true);
  });
  it("distinguishes paused playback from stopped playback", () => {
    const root = show(true, false, "file", 12);
    expect(root.findAllByType("span").some(span => span.children.includes("PAUSED"))).toBe(true);
  });
  it("reflects native playback state and opt-in test tone", () => {
    const root = show(true, true, "tone");
    expect(root.findByProps({ "aria-label": "Pause" }).props["aria-pressed"]).toBe(true);
    expect(root.findByProps({ "aria-label": "Standalone audio source" }).props.value).toBe("tone");
  });
});
