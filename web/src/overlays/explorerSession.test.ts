import { describe, expect, it } from "vitest";
import { explorerSession, patchExplorer } from "./explorerSession";

describe("explorer session", () => {
  it("keeps category, selection and scroll after close", () => {
    patchExplorer({ cat: "Distortion", sel: 4, folderScroll: 80, listScroll: 240 });
    expect(explorerSession.cat).toBe("Distortion");
    expect(explorerSession.sel).toBe(4);
    expect(explorerSession.folderScroll).toBe(80);
    expect(explorerSession.listScroll).toBe(240);
  });
});
