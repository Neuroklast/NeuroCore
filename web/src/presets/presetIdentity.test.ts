import { describe, expect, it } from "vitest";
import { factoryPresetId, migrateStarKeys, rowKey } from "./presetIdentity";

describe("preset identity", () => {
  it("keeps factory ids stable", () => {
    expect(factoryPresetId("Mesa High Gain")).toBe("factory:Mesa High Gain");
  });

  it("distinguishes duplicate names by id", () => {
    expect(rowKey({ id: "a", name: "Heavy Drive", packId: "one" })).toBe("a");
    expect(rowKey({ id: "b", name: "Heavy Drive", packId: "two" })).toBe("b");
  });

  it("migrates unique name favorites and skips clashes", () => {
    const rows = [
      { id: "id-1", name: "Airy" },
      { id: "id-2", name: "Heavy Drive" },
      { id: "id-3", name: "Heavy Drive" },
    ];
    const next = migrateStarKeys({ Airy: 1, "Heavy Drive": 5 }, rows);
    expect(next["id-1"]).toBe(1);
    expect(next.Airy).toBeUndefined();
    expect(next["Heavy Drive"]).toBe(5);
    expect(next["id-2"]).toBeUndefined();
  });
});
