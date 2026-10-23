import { describe, expect, it } from "vitest";
import { explorerSession, patchExplorer } from "./explorerSession";

describe("explorer session", () => {
  it("keeps category, selection and scroll after close", () => {
    patchExplorer({ cat: "Distortion", sources: ["Guitar"], types: ["Distortion"], selectedName: "Metal Scoop", folderScroll: 80, listScroll: 240 });
    expect(explorerSession.cat).toBe("Distortion");
    expect(explorerSession.sources).toEqual(["Guitar"]);
    expect(explorerSession.types).toEqual(["Distortion"]);
    expect(explorerSession.selectedName).toBe("Metal Scoop");
    expect(explorerSession.folderScroll).toBe(80);
    expect(explorerSession.listScroll).toBe(240);
  });
});
