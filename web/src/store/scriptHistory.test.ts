import { describe, expect, it } from "vitest";
import { alreadyLinked, scriptAfterDisconnect } from "../assemble/connectModel";
import { canUndoScript, pushScriptHistory, redoScript, resetScriptHistory, undoScript } from "./scriptHistory";

describe("demo formula history", () => {
  it("walks add then undo then redo", () => {
    resetScriptHistory();
    pushScriptHistory("stage1: y = x\n");
    const back = undoScript("stage1: y = x\nfilter1: type = lowpass\n");
    expect(back).toBe("stage1: y = x\n");
    const forth = redoScript(back!);
    expect(forth).toContain("filter1");
  });

  it("undo restores a deleted cable script and alreadyLinked stays honest", () => {
    resetScriptHistory();
    const linked = "stage1: y = x\nfilter1: type = lowpass; cutoff = 800\nout: main = 1\n";
    const edges = [{ source: "stage1", target: "filter1", sourceHandle: "src::out", targetHandle: "dst::in" }];
    expect(alreadyLinked(edges, "stage1", "filter1", "src::out", "dst::in")).toBe(true);

    pushScriptHistory(linked);
    expect(canUndoScript()).toBe(true);
    const cut = scriptAfterDisconnect(linked, "stage1", "filter1");
    expect(cut).not.toBe(linked);
    expect(cut).toContain("bus __park:");

    const restored = undoScript(cut);
    expect(restored).toBe(linked);
    expect(alreadyLinked(edges, "stage1", "filter1")).toBe(true);
    expect(alreadyLinked([{ ...edges[0]!, className: "temp" }], "stage1", "filter1")).toBe(false);
  });
});
