import { describe, expect, it } from "vitest";
import { chipBox, SOCKET_H } from "./chipLayout";
import {
  SOUTH_JACK_GAP,
  TITLE_H,
  TYPECODE_H,
  chipSpec,
  collapsedFace,
  paramJackSlots,
  typeCode,
} from "./chipSpec";

const CATALOG: Array<{
  id: string;
  label: string;
  params: string[];
}> = [
  { id: "filter", label: "Filter", params: ["type", "cutoff", "resonance", "channel"] },
  { id: "eq", label: "EQ", params: ["type", "freq", "q", "gain", "channel"] },
  { id: "delay", label: "Delay", params: ["time", "feedback", "mix", "sync", "pingpong"] },
  { id: "reverb", label: "Reverb", params: ["size", "decay", "damp", "mix"] },
  { id: "stage", label: "Drive", params: ["y", "channel"] },
  { id: "custom", label: "Custom", params: ["y"] },
  { id: "comp", label: "Comp", params: ["threshold", "ratio", "attack", "release"] },
  { id: "noisegate", label: "Gate", params: ["threshold", "attack", "release"] },
  { id: "limit", label: "Limit", params: ["ceiling", "release"] },
  { id: "ott", label: "OTT", params: ["depth", "time", "low", "mid", "high"] },
  { id: "ir", label: "IR", params: ["mix", "gain"] },
  { id: "env", label: "Env", params: ["type", "attack", "release", "depth"] },
  { id: "osc", label: "LFO", params: ["shape", "freq", "sync", "depth"] },
  { id: "octaver", label: "Octaver", params: ["sub", "up", "mix", "tone", "thresh"] },
  { id: "vocoder", label: "Vocoder", params: ["bands", "mix", "q"] },
  { id: "width", label: "Width", params: ["width", "delay", "bass"] },
  { id: "split_ms", label: "Split Mid/Side", params: [] },
  { id: "join_ms", label: "Join Mid/Side", params: [] },
  { id: "split_lr", label: "Split Left/Right", params: [] },
  { id: "join_lr", label: "Join Left/Right", params: [] },
  { id: "msplit", label: "Multiband Split", params: ["f1", "f2"] },
  { id: "bus", label: "Bus", params: ["name"] },
  { id: "join", label: "Join Signal", params: ["mix"] },
  { id: "send", label: "Send", params: ["kanal"] },
  { id: "in", label: "IN", params: [] },
  { id: "sidechain", label: "Sidechain", params: [] },
  { id: "out", label: "OUT", params: [] },
];

const dump = (id: string) => {
  try {
    const spec = chipSpec(id);
    return `${id} → ${spec.id} ${spec.label} params=${spec.paramJacks.join(",")} min=${spec.minBodyPx}`;
  } catch (err) {
    return `${id} threw ${err}`;
  }
};

describe("ChipSpec registry", () => {
  it("has a catalog row for every chip type", () => {
    for (const row of CATALOG) {
      const spec = chipSpec(row.id);
      expect(spec.id, dump(row.id)).toBe(row.id);
      expect(spec.label, dump(row.id)).toBe(row.label);
      expect(spec.paramJacks, dump(row.id)).toEqual(row.params);
      expect(spec.typeCodePrefix.length, dump(row.id)).toBeGreaterThan(0);
      expect(spec.minBodyPx, dump(row.id)).toBeGreaterThan(0);
    }
  });

  it("gives EQ the same channel enum as filter", () => {
    expect(chipSpec("eq").enums.channel).toEqual(chipSpec("filter").enums.channel);
    expect(chipSpec("eq").enums.channel).toEqual(["both", "left", "right", "mid", "side"]);
  });

  it("maps aliases onto the catalog ids", () => {
    expect(chipSpec("lfo").id).toBe("osc");
    expect(chipSpec("osc").id).toBe("osc");
    expect(chipSpec("widen").id).toBe("width");
    expect(chipSpec("xover").id).toBe("msplit");
    expect(chipSpec("crossover").id).toBe("msplit");
    expect(chipSpec("ngate").id).toBe("noisegate");
    expect(chipSpec("ms", { mode: "encode" }).id).toBe("split_ms");
    expect(chipSpec("ms", { mode: "decode" }).id).toBe("join_ms");
    expect(chipSpec("ms", { mode: "split" }).id).toBe("split_ms");
    expect(chipSpec("ms", { mode: "join" }).id).toBe("join_ms");
    expect(chipSpec("ms", { mode: "split", family: "lr" }).id).toBe("split_lr");
    expect(chipSpec("ms", { mode: "join", family: "lr" }).id).toBe("join_lr");
    expect(chipSpec("ms").label).toBe("Split Mid/Side");
  });

  it("gives filter a taller min body than reverb because its expanded param UI is taller", () => {
    const filter = chipSpec("filter");
    const reverb = chipSpec("reverb");
    expect(
      filter.minBodyPx,
      `filter ${filter.minBodyPx} params=${filter.paramJacks.length} vs reverb ${reverb.minBodyPx} params=${reverb.paramJacks.length}`,
    ).toBeGreaterThan(reverb.minBodyPx);
  });
});

describe("typeCode and collapsed face", () => {
  it("prints FILTER + FL-BP-900 for a bandpass at 900 Hz", () => {
    const spec = chipSpec("filter");
    const args = { type: "bandpass", cutoff: "900" };
    expect(typeCode(spec, args)).toBe("FL-BP-900");
    expect(collapsedFace("filter", args)).toEqual({ title: "FILTER", code: "FL-BP-900" });
  });

  it("prints WD-1.20 for width 1.20", () => {
    expect(typeCode(chipSpec("width"), { width: "1.20" })).toBe("WD-1.20");
    expect(typeCode(chipSpec("widen"), { width: "1.2" })).toBe("WD-1.20");
  });
});

describe("param jacks on the south edge", () => {
  it("labels every param jack and parks it on the bottom edge inside the clip", () => {
    for (const row of CATALOG) {
      const spec = chipSpec(row.id);
      const box = { w: 236, h: spec.minBodyPx };
      const slots = paramJackSlots(spec, box);
      expect(slots.map((s) => s.key), dump(row.id)).toEqual(spec.paramJacks);
      for (const slot of slots) {
        expect(slot.label.trim().length, `${row.id}.${slot.key}`).toBeGreaterThan(0);
        expect(slot.y, `${row.id}.${slot.key} y=${slot.y} h=${box.h}`).toBeGreaterThanOrEqual(box.h - 16);
        expect(slot.y, `${row.id}.${slot.key} y=${slot.y} h=${box.h}`).toBeLessThanOrEqual(box.h);
        expect(slot.x, `${row.id}.${slot.key} x`).toBeGreaterThan(0);
        expect(slot.x, `${row.id}.${slot.key} x`).toBeLessThan(box.w);
      }
    }
  });
});

describe("chipBox height ignores expand", () => {
  it("fits the expanded socket stack inside the chip for filter, ott, and octaver", () => {
    const header = TITLE_H + TYPECODE_H;
    const jacks = [
      { id: "in", label: "in", output: false, kind: "audio" as const },
      { id: "out", label: "out", output: true, kind: "audio" as const },
    ];
    for (const id of ["filter", "ott", "octaver"] as const) {
      const spec = chipSpec(id);
      const box = chipBox(id, jacks, true, spec.defaultArgs);
      const stack = spec.paramJacks.length * SOCKET_H + header + SOUTH_JACK_GAP;
      expect(
        box.h,
        `${id} stack=${stack} (n=${spec.paramJacks.length} * ${SOCKET_H} + ${header} + gap ${SOUTH_JACK_GAP}) h=${box.h} min=${spec.minBodyPx}`,
      ).toBeGreaterThanOrEqual(stack);
    }
  });

  it("keeps collapsed filter height equal to expanded", () => {
    const jacks = [
      { id: "in", label: "in", output: false, kind: "audio" as const },
      { id: "out", label: "out", output: true, kind: "audio" as const },
    ];
    const args = { type: "bandpass", cutoff: "900", resonance: "0.4", channel: "both" };
    const shut = chipBox("filter", jacks, false, args);
    const open = chipBox("filter", jacks, true, args);
    expect(shut.h).toBe(open.h);
    expect(shut.h).toBeGreaterThanOrEqual(chipSpec("filter").minBodyPx);
  });
});
