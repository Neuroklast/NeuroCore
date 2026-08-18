import { describe, expect, it } from "vitest";
import { pushScriptHistory, redoScript, resetScriptHistory, undoScript } from "./scriptHistory";

describe("demo formula history", () => {
  it("walks add then undo then redo", () => {
    resetScriptHistory();
    pushScriptHistory("stage1: y = x\n");
    const back = undoScript("stage1: y = x\nfilter1: type = lowpass\n");
    expect(back).toBe("stage1: y = x\n");
    const forth = redoScript(back!);
    expect(forth).toContain("filter1");
  });
});
