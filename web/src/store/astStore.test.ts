import { beforeEach, describe, expect, it } from "vitest";
import { useAstStore } from "./astStore";

const validAst = {
  version: 1 as const,
  leadingComments: [],
  params: [],
  nodes: [{ id: "stage1", type: "stage", busName: "main", args: { y: "x" }, trailingComment: "" }],
};

describe("astStore", () => {
  beforeEach(() => {
    useAstStore.setState({
      origin: "bridge",
      ast: null,
      lastValidAst: null,
      lastValidScript: "",
      script: "",
      diagnostics: [],
    });
  });

  it("stores ast from a successful bridge event", () => {
    useAstStore.getState().applyAstEvent({
      origin: "host",
      script: "stage1: y = x\n",
      astJson: JSON.stringify(validAst),
      diagnostics: [],
    });
    const s = useAstStore.getState();
    expect(s.lastValidAst?.nodes[0]?.id).toBe("stage1");
    expect(s.script).toContain("stage1");
    expect(s.origin).toBe("host");
  });

  it("keeps lastValidAst when compileResult is not ok", () => {
    useAstStore.getState().applyAstEvent({
      origin: "host",
      script: "stage1: y = x\n",
      astJson: JSON.stringify(validAst),
      diagnostics: [],
    });
    useAstStore.getState().applyCompileResult({
      origin: "editor",
      ok: false,
      diagnostics: [{ line: 1, column: 1, message: "Missing ':' on line 1" }],
    });
    const s = useAstStore.getState();
    expect(s.lastValidAst?.nodes[0]?.id).toBe("stage1");
    expect(s.diagnostics[0]?.line).toBe(1);
    expect(s.origin).toBe("editor");
  });

  it("can keep the draft script when the event originated in the editor", () => {
    useAstStore.getState().applyAstEvent({
      origin: "host",
      script: "stage1: y = x\n",
      astJson: JSON.stringify(validAst),
      diagnostics: [],
    });
    useAstStore.getState().setDraftScript("stage1: y = tanh(x)\n");
    useAstStore.getState().applyAstEvent({
      origin: "editor",
      script: "stage1: y = x\n",
      astJson: JSON.stringify(validAst),
      diagnostics: [],
    }, { updateScript: false });
    expect(useAstStore.getState().script).toContain("tanh");
    expect(useAstStore.getState().lastValidAst?.nodes[0]?.id).toBe("stage1");
  });
});
