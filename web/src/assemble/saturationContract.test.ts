import { describe, expect, it } from "vitest";
import { chipSpec } from "./chipSpec";
import { entryById } from "./blockCatalog";
import { ADDABLE_BLOCKS } from "./addBlock";

describe("saturation parameter contract", () => {
  it("exposes character and an audible default consistently", () => {
    const entry = entryById("saturate")!;
    const spec = chipSpec("saturate");
    expect(entry.params.find(p => p.key === "character")?.def).toBe("0.5");
    expect(spec.defaultArgs.character).toBe("0.5");
    expect(spec.ranges?.character).toEqual({ min: 0, max: 1 });
    expect(spec.defaultArgs.mix).toBe("1");
    expect(entry.params.find(p => p.key === "mix")?.def).toBe("1");
    expect(ADDABLE_BLOCKS.find(p => p.type === "saturate")?.args).toBe(entry.dslTemplate);
  });
});
