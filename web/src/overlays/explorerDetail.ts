import { chipSpec } from "../assemble/chipSpec";
import { isModulatorNode } from "../assemble/modulatorKinds";
import { parseDslSketch } from "../presets/parseDslSketch";

export type ExplorerScriptBlocks = {
  dsp: string[];
  mod: string[];
  chain: string[];
};

export function explorerScriptBlocks(script: string): ExplorerScriptBlocks {
  if (! script.trim()) {
    return { dsp: [], mod: [], chain: [] };
  }
  const { doc } = parseDslSketch(script);
  const dsp: string[] = [];
  const mod: string[] = [];
  const chain: string[] = [];
  const seenDsp = new Set<string>();
  const seenMod = new Set<string>();
  for (const node of doc.nodes) {
    const t = (node.type || "").toLowerCase();
    if (t === "in" || t === "out" || t === "bus") {
      continue;
    }
    const label = chipSpec(node.type, node.args).label;
    if (isModulatorNode(node)) {
      if (! seenMod.has(label)) {
        seenMod.add(label);
        mod.push(label);
      }
      continue;
    }
    chain.push(label);
    if (! seenDsp.has(label)) {
      seenDsp.add(label);
      dsp.push(label);
    }
  }
  return { dsp, mod, chain };
}
