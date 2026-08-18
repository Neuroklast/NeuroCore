import type { AstDocument } from "../bridge/ast";

const audio = (id: string, output: boolean) => ({ id, label: id, output, kind: "audio" });
const mod = (id: string, output: boolean) => ({ id, label: id, output, kind: "mod" });

export const demoAst = {
  script: "# phaser lab preview",
  doc: {
    version: 1,
    leadingComments: [],
    params: [
      { alias: "a", name: "Rate", min: 0.05, max: 6, isNote: false, noteWholes: [], noteLabels: [] },
      { alias: "b", name: "Depth", min: 200, max: 2500, isNote: false, noteWholes: [], noteLabels: [] },
      { alias: "c", name: "Center", min: 400, max: 4000, isNote: false, noteWholes: [], noteLabels: [] },
    ],
    nodes: [
      { id: "lfo1", type: "osc", busName: "mod", args: { freq: "a", shape: "sine" }, trailingComment: "", x: 16, y: 280, jacks: [mod("mod", true)] },
      { id: "stage1", type: "stage", busName: "main", args: { y: "tanh(x * d)" }, trailingComment: "", x: 268, y: 112, jacks: [audio("in", false), audio("out", true)] },
      { id: "filter1", type: "filter", busName: "main", args: { cutoff: "c + lfo1 * b", q: "f" }, trailingComment: "", x: 628, y: 112, jacks: [audio("in", false), mod("lfo1", false), audio("out", true)] },
    ],
    edges: [
      { from: "IN", to: "stage1", kind: "audio", fromJack: "out", toJack: "in" },
      { from: "stage1", to: "filter1", kind: "audio", fromJack: "out", toJack: "in" },
      { from: "filter1", to: "OUT", kind: "audio", fromJack: "out", toJack: "in" },
    ],
    inJacks: [audio("out", true)],
  } satisfies AstDocument,
};
