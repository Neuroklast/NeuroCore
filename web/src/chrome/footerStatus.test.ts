import { readFileSync } from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { describe, expect, it } from "vitest";
import { footerBuf, footerSr } from "./footerStatus";

describe("footer status slots", () => {
  it("does not invent a sample-rate token when the host has not reported one", () => {
    expect(footerSr(0)).toBe("—");
    expect(footerSr(48000)).toBe("48000");
    expect(footerBuf(0)).toBe("—");
    expect(footerBuf(512)).toBe("512");
    expect(`${footerSr(0)} ${footerBuf(0)}`).not.toMatch(/32f/);
  });

  it("Footer selects status fields instead of the whole host store", () => {
    const here = path.dirname(fileURLToPath(import.meta.url));
    const src = readFileSync(path.join(here, "Chrome.tsx"), "utf8");
    const foot = src.slice(src.indexOf("export function Footer"));
    expect(foot).not.toMatch(/useHostStore\(\)/);
    expect(foot).toContain("useHostStore((s) => s.cpu)");
  });
});
