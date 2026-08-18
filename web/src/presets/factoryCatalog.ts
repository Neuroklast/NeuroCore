import factory from "./factoryCatalog.gen.json";

/** Legacy names that survived the quality cull. */
export const restoredLegacyNames = [
  "Acoustic Body",
  "Arp Shimmer",
  "Bitcrush Glitch",
  "Blues Break OD",
  "Chug Palm Mute",
  "Dark Atmosphere",
  "Harmonic Exciter",
  "Loudness Safe",
  "Low End Control",
  "Metal Scoop",
  "Pluck Bite",
  "Podcast Voice",
  "Riff Sustain",
  "Sub Weight",
  "Tape Wobble",
  "Telephone EQ",
  "Vocal De-Ess",
  "Vocal Double",
  "Vocal Presence",
  "Vocal Warmth",
  "Vowel Filter",
] as const;

export type FactoryKnob = {
  id: string;
  name: string;
  min: number;
  max: number;
  default: number;
};

export type FactoryRow = {
  name: string;
  category: string;
  description: string;
  author: string;
  factory: boolean;
  tags: string[];
  script: string;
  mix: number;
  knobs: FactoryKnob[];
};

const rows: FactoryRow[] = factory
  .filter((p) => typeof p?.name === "string" && p.name.length > 0)
  .map((p) => ({
    name: p.name,
    category: String(p.category ?? "Factory"),
    description: String(p.description ?? ""),
    author: "Neuroklast",
    factory: true,
    tags: Array.isArray(p.tags) ? p.tags.map(String) : [],
    script: String(p.script ?? ""),
    mix: Number((p as { mix?: number }).mix ?? 1),
    knobs: Array.isArray((p as { knobs?: FactoryKnob[] }).knobs)
      ? (p as { knobs: FactoryKnob[] }).knobs.map((k) => ({
          id: String(k.id ?? ""),
          name: String(k.name ?? ""),
          min: Number(k.min ?? 0),
          max: Number(k.max ?? 1),
          default: Number(k.default ?? 0),
        }))
      : [],
  }));

export function factoryRows(): FactoryRow[] {
  return rows;
}

export function factoryExplorerRows(): Array<Omit<FactoryRow, "script" | "knobs" | "mix">> {
  return rows.map((row) => ({
    name: row.name,
    category: row.category,
    description: row.description,
    author: row.author,
    factory: row.factory,
    tags: row.tags,
  }));
}

export function findFactory(name: string): FactoryRow | undefined {
  return rows.find((p) => p.name === name);
}
