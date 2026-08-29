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

  it("mute comments do not replace the visual AST", () => {
    const json = JSON.stringify(validAst);
    useAstStore.getState().applyAstEvent({
      origin: "preset",
      script: "stage1: y = x\n",
      astJson: json,
      diagnostics: [],
    });
    const ast = useAstStore.getState().ast;
    useAstStore.getState().applyAstEvent({
      origin: "canvas",
      script: "# nk-ms stage1: y = x\n",
      astJson: JSON.stringify({ ...validAst, nodes: [], edges: [] }),
      diagnostics: [],
    });
    expect(useAstStore.getState().ast).toBe(ast);
    expect(useAstStore.getState().ast?.nodes).toHaveLength(1);
    expect(useAstStore.getState().script).toContain("# nk-ms");
    expect(useAstStore.getState().origin, "mute must not flip origin or Circuit rehydrates").toBe("preset");
  });

  it("a second mute compile echo with empty nodes still keeps the visual chips", () => {
    const json = JSON.stringify(validAst);
    useAstStore.getState().applyAstEvent({
      origin: "preset",
      script: "stage1: y = x\n",
      astJson: json,
      diagnostics: [],
    });
    const muted = "# nk-ms stage1: y = x\n";
    const empty = JSON.stringify({ ...validAst, nodes: [], edges: [] });
    useAstStore.getState().applyAstEvent({
      origin: "canvas",
      script: muted,
      astJson: empty,
      diagnostics: [],
    });
    useAstStore.getState().applyAstEvent({
      origin: "canvas",
      script: muted,
      astJson: empty,
      diagnostics: [],
    });
    expect(useAstStore.getState().ast?.nodes).toHaveLength(1);
    expect(useAstStore.getState().ast?.nodes[0]?.id).toBe("stage1");
  });
});
