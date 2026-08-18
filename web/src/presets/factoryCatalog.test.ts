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
  });
});
