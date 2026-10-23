import { afterEach, describe, expect, it } from "vitest";
import { createElement } from "react";
import { act, create, type ReactTestRenderer } from "react-test-renderer";
import { SettingsBody } from "./Overlays";
import { useHostStore } from "../store/hostStore";

const initial = useHostStore.getState();
const mounted: ReactTestRenderer[] = [];

function settingsView(override: Record<string, unknown> = {}) {
  useHostStore.setState({
    standalone: false, tempoSource: "HOST", bpm: 128,
    live: false, licensed: true, licenseEmail: "", cables: "wave",
    ...override,
  });
  let renderer!: ReactTestRenderer;
  act(() => { renderer = create(createElement(SettingsBody)); });
  mounted.push(renderer);
  return renderer.root;
}

function buttonNames(root: ReturnType<typeof settingsView>) {
  return root.findAllByType("button").map((button) => button.children.join(""));
}

afterEach(() => {
  act(() => { mounted.splice(0).forEach((renderer) => renderer.unmount()); });
  useHostStore.setState(initial);
});

describe("settings dialog", () => {
  it("groups controls, marks selected values and contains only one license action", () => {
    const root = settingsView();
    expect(root.findAllByProps({ className: "nk-settings-grid" })).toHaveLength(1);
    expect(root.findAllByProps({ "aria-label": "Processing mode" })).toHaveLength(1);
    const studio = root.findAllByType("button").find((button) => button.children.includes("Studio"));
    expect(studio?.props["aria-pressed"]).toBe(true);
    expect(root.findAllByProps({ "aria-label": "Color theme" })).toHaveLength(1);
    expect(buttonNames(root).filter((label) => label === "Manage license")).toHaveLength(1);
    expect(buttonNames(root)).not.toContain("Install .lic");
    expect(JSON.stringify(root.toJSON())).not.toContain("CPU in the status bar");
  });

  it("shows audio device settings in standalone but not inside a DAW", () => {
    expect(buttonNames(settingsView({ standalone: false }))).not.toContain("Audio device settings");
    expect(buttonNames(settingsView({ standalone: true }))).toContain("Audio device settings");
  });

  it("makes host BPM read-only and manual BPM editable", () => {
    const host = settingsView({ tempoSource: "HOST", bpm: 137 }).findByType("input");
    expect(host.props["aria-label"]).toBe("Host BPM");
    expect(host.props.disabled).toBe(true);
    expect(host.props.value).toBe("137");
    const manual = settingsView({ tempoSource: "USER", bpm: 132 }).findByType("input");
    expect(manual.props["aria-label"]).toBe("Manual BPM");
    expect(manual.props.disabled).toBe(false);
    expect(manual.props.value).toBe("132");
  });

  it("offers a discard warning and displays the license owner", () => {
    const root = settingsView({ discardPrompt: false, licenseEmail: "producer@example.com" });
    const snapshot = JSON.stringify(root.toJSON());
    expect(snapshot).toContain("Licensed to producer@example.com");
    expect(root.findAllByProps({ "aria-label": "Warn before discarding edits" })).toHaveLength(1);
    expect(buttonNames(root)).toContain("No warning");
    expect(snapshot).toContain("without confirmation");
  });
});
