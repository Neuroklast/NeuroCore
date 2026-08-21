import { describe, expect, it } from "vitest";
import { crtHost } from "./CrtFx";

describe("CRT layer host", () => {
  it("keeps scan on the OS and confines vignette to the workspace pane", () => {
    expect(crtHost("vignette")).toBe("pane");
    expect(crtHost("techNoise")).toBe("pane");
    expect(crtHost("scan")).toBe("os");
    expect(crtHost("chroma")).toBe("os");
    expect(crtHost("sweep")).toBe("os");
    expect(crtHost("bloom")).toBe("os");
  });
});
