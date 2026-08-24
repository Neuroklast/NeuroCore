import { describe, expect, it } from "vitest";
import { bundledMonacoConfig } from "./monacoLoader";

describe("monaco loader", () => {
  it("hands the bundled editor to the loader instead of jsDelivr", () => {
    const monaco = { editor: {} };
    expect(bundledMonacoConfig(monaco)).toEqual({ monaco });
    expect(JSON.stringify(bundledMonacoConfig(monaco))).not.toMatch(/jsdelivr|unpkg|cdn/i);
  });
});
