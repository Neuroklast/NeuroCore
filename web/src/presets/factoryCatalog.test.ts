import { describe, expect, it } from "vitest";
import { factoryRows, findFactory, restoredLegacyNames } from "./factoryCatalog";

describe("factory catalog", () => {
  it("ships every factory preset including the restored legacy set", () => {
    const rows = factoryRows();
    expect(rows.length).toBeGreaterThanOrEqual(200);
    const names = new Set(rows.map((r) => r.name));
    expect(names.size).toBe(rows.length);
    for (const name of restoredLegacyNames) {
      expect(names.has(name)).toBe(true);
    }
    expect(findFactory("Airy Clean")?.script.length).toBeGreaterThan(0);
    expect(findFactory("Blues Break OD")?.script.length).toBeGreaterThan(0);
    expect(findFactory("Mesa High Gain")?.script.length).toBeGreaterThan(0);
    const vocals = rows.filter((r) => r.category === "Vocals");
    expect(vocals.length).toBeGreaterThanOrEqual(40);
    for (const name of ["Vocal De-Ess", "Vocal Double", "Vocal Presence", "Vocal Warmth", "Vocal Chain Pro", "Vocal Send"]) {
      expect(names.has(name), name).toBe(true);
    }
  });

  it("ships Club flagship presets for 909 hardcore and techno transients", () => {
    const club = factoryRows().filter((r) => r.category === "Club");
    expect(club.length).toBeGreaterThanOrEqual(8);
    const names = new Set(club.map((r) => r.name));
    for (const name of ["909 Newstyle", "Crisp Brick", "Techno Snap", "Side Scream", "Kick Rumble", "Hardcore Clip"]) {
      expect(names.has(name), name).toBe(true);
      const row = findFactory(name);
      expect(row?.script.length).toBeGreaterThan(40);
      expect(row?.category).toBe("Club");
    }
    expect(findFactory("909 Newstyle")?.script).toMatch(/env1/);
    expect(findFactory("909 Newstyle")?.script).toMatch(/ms1|mode = encode/);
    expect(findFactory("Techno Snap")?.script).toMatch(/env1/);
    expect(findFactory("Side Scream")?.script).toMatch(/channel = side/);
    expect(findFactory("Crisp Brick")?.script).toMatch(/hardclip|diode|softclip/);
  });

  it("ships the NeuroCore PDF chain set without dropping Vocals", () => {
    const names = new Set(factoryRows().map((r) => r.name));
    for (const name of [
      "Offbeat Gallop",
      "Schranz Multiband",
      "Koren Stack Cab",
      "Streaming Ceiling",
      "FET All In",
      "Precision Multiband",
      "British Desk EQ",
      "Passive Low Trick",
      "Ladder Sweep",
      "Octave Cloud",
      "Blackwall Space",
      "Click Sustain",
    ]) {
      expect(names.has(name), name).toBe(true);
      expect(findFactory(name)?.script).not.toMatch(/How it sounds/i);
    }
    expect(factoryRows().filter((r) => r.category === "Vocals").length).toBeGreaterThanOrEqual(40);
  });

  it("writes the sound as a comment, never the How-it-sounds label or mojibake arrows", () => {
    for (const row of factoryRows()) {
      expect(row.script, row.name).not.toMatch(/How it sounds/i);
      expect(`${row.description}\n${row.script}`, row.name).not.toMatch(/â.|Ã.|Â./);
    }
  });
});
