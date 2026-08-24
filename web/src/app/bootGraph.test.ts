import { describe, expect, it } from "vitest";
import appSrc from "./App.tsx?raw";
import hackSrc from "../hack/HackView.tsx?raw";

describe("boot graph", () => {
  it("does not load Monaco or HackView until Terminal mounts", () => {
    expect(appSrc).not.toMatch(/monacoEnv/);
    expect(appSrc).not.toMatch(/from ["']\.\.\/hack\/HackView["']/);
    expect(appSrc).toMatch(/lazy\(\s*\(\)\s*=>\s*import\(\s*["']\.\.\/hack\/HackView["']\s*\)/);
    expect(appSrc).toMatch(/terminalMounted/);
    expect(hackSrc).toMatch(/monacoEnv/);
  });
});
