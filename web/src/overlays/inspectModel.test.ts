import { describe, expect, it } from "vitest";
import type { AstDocument } from "../bridge/ast";
import {
  boundKnobRows,
  clampInspectArg,
  inspectArgFields,
  inspectBlurb,
  inspectEnumOptions,
  inspectNoteChoices,
  inspectSnapNote,
  inspectValueFromNote,
  inspectRows,
} from "./inspectModel";

describe("inspect rows", () => {
  it("lists args, jacks, bound params and referenced variables", () => {
    const doc: AstDocument = {
      version: 1,
      leadingComments: [],
      params: [{ alias: "b", name: "Depth", min: 200, max: 2500, isNote: false, noteWholes: [], noteLabels: [] }],
      nodes: [
        {
          id: "lfo1",
          type: "osc",
          busName: "mod",
          args: { freq: "a" },
          trailingComment: "",
        },
        {
          id: "filter1",
          type: "filter",
          busName: "main",
          args: { cutoff: "c + lfo1 * b", q: "f" },
          trailingComment: "swirl",
          jacks: [
            { id: "in", label: "in", output: false, kind: "audio" },
            { id: "lfo1", label: "lfo1", output: false, kind: "mod" },
            { id: "out", label: "out", output: true, kind: "audio" },
          ],
        },
      ],
    };
    const rows = inspectRows(doc.nodes[1], doc);
    expect(rows.some((r) => r.group === "arg" && r.key === "cutoff")).toBe(true);
    expect(rows.some((r) => r.group === "jack" && r.key === "lfo1" && r.value.includes("mod"))).toBe(true);
    expect(rows.some((r) => r.group === "param" && r.key === "b")).toBe(true);
    expect(rows.some((r) => r.group === "var" && r.key === "lfo1")).toBe(true);
    expect(rows.some((r) => r.group === "meta" && r.key === "comment")).toBe(true);
  });
});

describe("inspect enums / ranges / blurb / bindings", () => {
  it("filter type options are lowpass/highpass/bandpass/allpass from chipSpec", () => {
    expect(inspectEnumOptions("filter", "type")).toEqual(["lowpass", "highpass", "bandpass", "allpass"]);
    expect(inspectEnumOptions("filter", "cutoff")).toEqual([]);
  });

  it("clamps numeric args to chipSpec.ranges (cutoff 10e9 → max)", () => {
    expect(clampInspectArg("filter", "cutoff", "10e9")).toBe("20000");
    expect(clampInspectArg("filter", "cutoff", "1")).toBe("20");
    expect(clampInspectArg("filter", "cutoff", "1000")).toBe("1000");
    expect(clampInspectArg("filter", "cutoff", "a")).toBe("a");
    expect(clampInspectArg("filter", "cutoff", "c + lfo1")).toBe("c + lfo1");
  });

  it("exposes chipSpec blurb", () => {
    expect(inspectBlurb("filter")).toBe("State-variable filter, cutoff in Hz.");
  });

  it("bound letter appears with unit and two decimal places", () => {
    const doc: AstDocument = {
      version: 1,
      leadingComments: [],
      params: [
        { alias: "a", name: "Cutoff", min: 200, max: 2500, isNote: false, noteWholes: [], noteLabels: [] },
      ],
      nodes: [
        {
          id: "filter1",
          type: "filter",
          busName: "main",
          args: { type: "lowpass", cutoff: "a", resonance: "0.4", channel: "both" },
          trailingComment: "",
        },
      ],
    };
    const knobs = boundKnobRows(doc.nodes[0], doc);
    expect(knobs.some((k) => k.letter === "a")).toBe(true);
    const a = knobs.find((k) => k.letter === "a")!;
    expect(a.unit).toBe("Hz");
    expect(a.value).toContain("200.00");
    expect(a.value).toContain("2500.00");
    expect(a.value).toContain("Hz");
  });

  it("offers VALUE/NOTE on time fields and clamps notes to chipSpec range", () => {
    const gate = inspectArgFields({
      id: "gate1",
      type: "gate",
      busName: "main",
      args: { hold: "0.03", threshold: "-46", attack: "0.01", release: "0.1" },
      trailingComment: "",
    });
    const hold = gate.find((f) => f.key === "hold")!;
    expect(hold.timing).toBe(true);
    expect(hold.unit).toBe("s");
    const attack = gate.find((f) => f.key === "attack")!;
    expect(attack.timing).toBe(true);
    expect(inspectNoteChoices("s", 0.001, 1, 120)).toContain("1/16");
    expect(inspectNoteChoices("s", 0.001, 1, 120)).not.toContain("8/1");
    expect(inspectValueFromNote("1/16", "s", 120)).toBe("0.125");
    expect(inspectSnapNote("0.13", "s", 0.001, 1, 120)).toBe("1/16");
    const delay = inspectNoteChoices("ms", 1, 2000, 120);
    expect(delay).toContain("1/1");
    expect(delay).not.toContain("8/1");
    const lfo = inspectNoteChoices("Hz", 0.01, 40, 120);
    expect(lfo).toContain("8/1");
    expect(lfo).toContain("1/16");
    const osc = inspectArgFields({
      id: "osc1",
      type: "osc",
      busName: "mod",
      args: { freq: "2" },
      trailingComment: "",
    });
    expect(osc.find((f) => f.key === "freq")!.timing).toBe(true);
    expect(inspectArgFields({
      id: "filter1",
      type: "filter",
      busName: "main",
      args: { cutoff: "1000" },
      trailingComment: "",
    })[0]!.timing).toBe(false);
  });

  it("shows note tokens for a note-range bound knob", () => {
    const doc: AstDocument = {
      version: 1,
      leadingComments: [],
      params: [
        { alias: "a", name: "Time", min: 1, max: 0.0625, isNote: true, noteWholes: [1, 0.0625], noteLabels: ["1/1", "1/16"] },
      ],
      nodes: [
        {
          id: "delay1",
          type: "delay",
          busName: "main",
          args: { time: "a" },
          trailingComment: "",
        },
      ],
    };
    const a = boundKnobRows(doc.nodes[0], doc).find((k) => k.letter === "a")!;
    expect(a.value).toContain("1/1");
    expect(a.value).toContain("1/16");
    expect(a.value).not.toContain("0.06");
  });
});
