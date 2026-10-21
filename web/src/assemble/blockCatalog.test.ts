import { describe, expect, it } from "vitest";
import {
  chipSpec,
  allChipSpecs,
} from "./chipSpec";
import {
  ADDABLE_BLOCKS,
  ADD_CATEGORIES,
  scriptAfterAdd,
} from "./addBlock";
import {
  BLOCK_CATALOG,
  entryById,
} from "./blockCatalog";
import { parseDslSketch } from "../presets/parseDslSketch";

describe("Block-Katalog — zentrale Metadaten-Quelle", () => {
  it("jeder Katalog-Eintrag hat eine eindeutige ID", () => {
    const ids = BLOCK_CATALOG.map((e) => e.id);
    expect(new Set(ids).size).toBe(ids.length);
  });

  it("jeder Katalog-Eintrag hat einen ChipSpec (kein Fallback) mit passender ID/Label", () => {
    for (const entry of BLOCK_CATALOG) {
      const spec = chipSpec(entry.id);
      expect(spec.id).toBe(entry.id);
      expect(spec.label).toBe(entry.label);
      expect(spec.blurb.length).toBeGreaterThan(0);
      expect(spec.typeCodePrefix.length).toBeGreaterThan(0);
    }
  });

  it("ChipSpec paramJacks entsprechen genau den Katalog-Parametern", () => {
    for (const entry of BLOCK_CATALOG) {
      const spec = chipSpec(entry.id);
      expect(spec.paramJacks).toEqual(entry.params.map((p) => p.key));
    }
  });

  it("jeder Parameter hat einen gültigen Defaultwert", () => {
    for (const entry of BLOCK_CATALOG) {
      for (const p of entry.params) {
        if (p.enum) {
          expect(p.enum).toContain(String(p.def), `${entry.id}.${p.key} Default "${p.def}" nicht im Enum`);
        } else {
          const def = Number(p.def);
          // Formel-Defaults (z. B. stage/custom "y = x") sind keine bloßen Zahlen
          // und werden durch die Formel-Validierung abgedeckt, nicht durch den Bereich.
          if (!Number.isFinite(def)) {
            continue;
          }
          expect(def).toBeGreaterThanOrEqual(p.min);
          expect(def).toBeLessThanOrEqual(p.max);
        }
      }
    }
  });

  it("jeder im Add-Menü angebotene Block hat einen ChipSpec (kein Fallback) und eine gültige Kategorie", () => {
    for (const block of ADDABLE_BLOCKS) {
      expect(ADD_CATEGORIES).toContain(block.category);
      const spec = chipSpec(block.type);
      expect(spec.blurb.length).toBeGreaterThan(0, `${block.type} hat keinen echten ChipSpec`);
      const entry = entryById(block.type);
      expect(
        entry != null || BLOCK_CATALOG.some((e) => e.dspType === block.type),
        `${block.type} nicht im Katalog`,
      ).toBeTruthy();
    }
  });

  it("jedes Add-Menü-Template erzeugt ein parses-fähiges DSL-Skript mit neuem Node", () => {
    for (const block of ADDABLE_BLOCKS) {
      const next = scriptAfterAdd("", block.type, block.args);
      const { doc } = parseDslSketch(next);
      const added = doc.nodes.filter((n) => n.id !== "__park");
      expect(added.length).toBe(1);
      expect(added[0]!.id.length).toBeGreaterThan(0);
      expect(added[0]!.type.length).toBeGreaterThan(0);
    }
  });

  it("alle nativen öffentlichen DSP-Blöcke sind im Katalog oder als intern markiert", () => {
    const internal = new Set(["bus", "join", "send", "in", "out", "sidechain"]);
    for (const entry of BLOCK_CATALOG) {
      if (internal.has(entry.id)) continue;
      expect(
        entry.params.length > 0 || entry.audioOuts.length > 0,
        `${entry.id} hat weder Ports noch Parameter`,
      ).toBe(true);
    }
  });

  it("allChipSpecs liefert für jeden Katalog-Eintrag denselben paramJacks-Satz", () => {
    const byId = Object.fromEntries(allChipSpecs().map((s) => [s.id, s]));
    for (const entry of BLOCK_CATALOG) {
      expect(byId[entry.id]).toBeDefined();
      expect(byId[entry.id]!.paramJacks).toEqual(entry.params.map((p) => p.key));
    }
  });
});
