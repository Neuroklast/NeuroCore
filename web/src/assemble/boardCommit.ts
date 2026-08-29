import { getNativeFunction, hasJuceBridge } from "../bridge/juce";
import { useAstStore } from "../store/astStore";
import type { AstEdge } from "../bridge/ast";
import { publishScript } from "./addBlock";
import { fitCamera } from "./boardCamera";
import { graphToLayout, type BoardPort } from "./boardModel";
import { parseRoutePath } from "./boardPath";
import { edgesAfterConnect, edgesAfterCutPort } from "./boardConnect";
import { scriptAfterDisconnect } from "./connectModel";
import { useBoardStore } from "./boardStore";
import { requestGraphLayout } from "./layout/layoutClient";

function astEdgesAfterConnect(edges: AstEdge[], src: BoardPort, dst: BoardPort): AstEdge[] {
  const kind = src.kind === "mod" ? "mod" : "audio";
  return [
    ...edges.filter((e) => ! (e.to === dst.nodeId && (e.toJack || "in") === dst.jackId)),
    { from: src.nodeId, to: dst.nodeId, kind, fromJack: src.jackId, toJack: dst.jackId },
  ];
}

function astEdgesAfterCut(edges: AstEdge[], port: BoardPort): AstEdge[] {
  return edges.filter((e) => {
    const from = e.from === port.nodeId && (e.fromJack || "out") === port.jackId && port.east;
    const to = e.to === port.nodeId && (e.toJack || "in") === port.jackId && ! port.east;
    return ! from && ! to;
  });
}

function pushAst(nextEdges: AstEdge[]): void {
  const cur = useAstStore.getState();
  if (! cur.ast) {
    return;
  }
  useAstStore.getState().applyAstEvent({
    origin: "canvas",
    script: cur.lastValidScript || cur.script,
    astJson: JSON.stringify({ ...cur.ast, edges: nextEdges }),
    diagnostics: [],
  }, { updateScript: false });
}

export async function layoutBoard(
  mode: "ARRANGE" | "COMPACT" | "REROUTE",
  view: { w: number; h: number },
  opts: { force?: boolean } = {},
): Promise<void> {
  const epoch = useBoardStore.getState().layoutEpoch;
  if (mode !== "REROUTE") {
    useBoardStore.setState({ layoutBusy: true });
  }
  try {
    const g = useBoardStore.getState();
    const payload = graphToLayout(g);
    const laid = await requestGraphLayout(mode, payload.nodes, payload.edges, view);
    const live = useBoardStore.getState();
    if (live.layoutEpoch !== epoch) {
      return;
    }
    const commanded = opts.force === true && mode !== "REROUTE";
    if (mode !== "REROUTE" && live.userMoved && ! commanded) {
      return;
    }
    if (mode !== "REROUTE" && ! commanded && live.commandedLayout) {
      return;
    }
    const routes: Record<string, Array<{ x: number; y: number }>> = {};
    for (const [id, d] of Object.entries(laid.edgePaths)) {
      routes[id] = parseRoutePath(d);
    }
    useBoardStore.getState().applyLayout(laid.nodes, routes, epoch);
    if (commanded) {
      useBoardStore.setState({
        userMoved: false,
        commandedLayout: mode === "COMPACT" ? "COMPACT" : "ARRANGE",
      });
    }
    if (mode !== "REROUTE") {
      const next = useBoardStore.getState();
      useBoardStore.getState().setCamera(fitCamera(Object.values(next.nodes), view));
    }
  } finally {
    if (mode !== "REROUTE" && useBoardStore.getState().layoutEpoch === epoch) {
      useBoardStore.setState({ layoutBusy: false });
    }
  }
}

export function rerouteBoard(): void {
  const epoch = useBoardStore.getState().layoutEpoch;
  const g = useBoardStore.getState();
  const payload = graphToLayout(g);
  void requestGraphLayout("REROUTE", payload.nodes, payload.edges).then((laid) => {
    if (useBoardStore.getState().layoutEpoch !== epoch) {
      return;
    }
    const routes: Record<string, Array<{ x: number; y: number }>> = {};
    for (const [id, d] of Object.entries(laid.edgePaths)) {
      routes[id] = parseRoutePath(d);
    }
    useBoardStore.getState().applyLayout(laid.nodes, routes, epoch);
  }).catch(() => undefined);
}

export function commitBoardConnect(src: BoardPort, dst: BoardPort): void {
  const g = useBoardStore.getState();
  useBoardStore.getState().setEdges(edgesAfterConnect(g, src, dst));
  const ast = useAstStore.getState().ast;
  if (ast) {
    pushAst(astEdgesAfterConnect(ast.edges ?? [], src, dst));
  }
  if (hasJuceBridge()) {
    void getNativeFunction("graphOp")({
      origin: "canvas",
      op: "connect",
      from: src.nodeId,
      fromJack: src.jackId,
      to: dst.nodeId,
      toJack: dst.jackId,
    });
  }
  rerouteBoard();
}

export function commitBoardCut(port: BoardPort): void {
  const g = useBoardStore.getState();
  const doomed = Object.values(g.edges).filter((e) => e.sourcePortId === port.id || e.targetPortId === port.id);
  useBoardStore.getState().setEdges(edgesAfterCutPort(g, port.id));
  const ast = useAstStore.getState().ast;
  if (ast) {
    pushAst(astEdgesAfterCut(ast.edges ?? [], port));
  }
  const cur = useAstStore.getState();
  let script = cur.lastValidScript || cur.script;
  for (const e of doomed) {
    script = scriptAfterDisconnect(script, e.sourceNodeId, e.targetNodeId);
  }
  if (script !== (cur.lastValidScript || cur.script)) {
    publishScript(script, "canvas");
  }
  if (hasJuceBridge()) {
    for (const e of doomed) {
      void getNativeFunction("graphOp")({
        origin: "canvas",
        op: "disconnect",
        from: e.sourceNodeId,
        fromJack: g.ports[e.sourcePortId]?.jackId ?? "out",
        to: e.targetNodeId,
        toJack: g.ports[e.targetPortId]?.jackId ?? "in",
      });
    }
  }
  rerouteBoard();
}
