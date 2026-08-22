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
});
