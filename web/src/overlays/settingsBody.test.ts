import { afterEach, describe, expect, it } from "vitest";
import { createElement } from "react";
import { renderToStaticMarkup } from "react-dom/server";
import { SettingsBody } from "./Overlays";
import { useHostStore } from "../store/hostStore";

const initial = useHostStore.getState();
function settingsHtml(override: Record<string, unknown> = {}) {
  useHostStore.setState({
    standalone: false, tempoSource: "HOST", bpm: 128,
    live: false, licensed: true, licenseEmail: "",
    ...override,
  });
  return renderToStaticMarkup(createElement(SettingsBody));
}
afterEach(() => useHostStore.setState(initial));

describe("settings dialog", () => {
  it("groups controls, marks selected values and contains only one license action", () => {
    const html = settingsHtml();
    expect(html).toContain('data-settings=""');
    expect(html).toContain('class="nk-settings-grid"');
    expect(html).toContain('aria-label="Processing mode"');
    expect(html).toMatch(/aria-pressed="true"[^>]*>Studio<\/button>/);
    expect(html).toContain('aria-label="Color theme"');
    expect(html.match(/Manage license/g)).toHaveLength(1);
    expect(html).not.toContain("Install .lic");
    expect(html).not.toContain("CPU in the status bar");
  });

  it("shows audio device settings in standalone but not inside a DAW", () => {
    expect(settingsHtml({ standalone: false })).not.toContain("Audio device settings");
    expect(settingsHtml({ standalone: true })).toContain("Audio device settings");
  });

  it("makes host BPM read-only and manual BPM editable", () => {
    const host = settingsHtml({ tempoSource: "HOST", bpm: 137 });
    expect(host).toMatch(/aria-label="Host BPM"[^>]*disabled=""[^>]*value="137"/);
    const manual = settingsHtml({ tempoSource: "USER", bpm: 132 });
    expect(manual).toMatch(/aria-label="Manual BPM"[^>]*value="132"/);
    expect(manual).not.toMatch(/aria-label="Manual BPM"[^>]*disabled=""/);
  });

  it("offers a clear discard warning and displays license owner when known", () => {
    const html = settingsHtml({ discardPrompt: false, licenseEmail: "producer@example.com" });
    expect(html).toContain("Licensed to producer@example.com");
    expect(html).toContain("Warn before discarding edits");
    expect(html).toContain("No warning");
    expect(html).toContain("without confirmation");
  });
});
