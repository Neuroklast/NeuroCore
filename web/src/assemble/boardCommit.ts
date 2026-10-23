import { getNativeFunction, hasJuceBridge } from "../bridge/juce";
import { useAstStore } from "../store/astStore";
import { beginPatchTx } from "../store/patchWorkspace";
import type { AstEdge } from "../bridge/ast";
import { publishScript } from "./addBlock";
import { camerasDiffer, fitBoardCamera, shouldFitCameraOnLateLayout } from "./boardCamera";
import { circuitHasOverlay } from "../chrome/shortcuts";
import { graphOpEndpoints, graphToLayout, portIsOutput, type BoardPort } from "./boardModel";
import { parseRoutePath } from "./boardPath";
import { edgesAfterCutPort } from "./boardConnect";
import { scriptAfterConnect, scriptAfterDisconnect } from "./connectModel";
import { useBoardStore } from "./boardStore";
import { requestGraphLayout } from "./layout/layoutClient";

function astEdgesAfterCut(edges: AstEdge[], port: BoardPort): AstEdge[] {
  const semantic = graphOpEndpoints(port);
  return edges.filter((e) => {
    const from = e.from === semantic.nodeId && (e.fromJack || "out") === semantic.jackId && portIsOutput(port);
    const to = e.to === semantic.nodeId && (e.toJack || "in") === semantic.jackId && ! portIsOutput(port);
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

function reportRoutingError(error: unknown): void {
  useAstStore.setState({ diagnostics: [{line: 1, column: 1,
    message: error instanceof Error ? error.message : "Connections could not be routed."}] });
}

let layoutBusyOwner = 0;

export async function layoutBoard(
  mode: "ARRANGE" | "COMPACT" | "REROUTE",
  view: { w: number; h: number },
  opts: { force?: boolean; overlayOpen?: boolean } = {},
): Promise<void> {
  const epoch = useBoardStore.getState().layoutEpoch;
  const owns = mode !== "REROUTE";
  if (owns) {
    layoutBusyOwner += 1;
    useBoardStore.setState({ layoutBusy: true });
  }
  const token = layoutBusyOwner;
  try {
    const g = useBoardStore.getState();
    const cameraAtStart = g.camera;
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
    const next = useBoardStore.getState();
    const overlayOpen = opts.overlayOpen ?? circuitHasOverlay();
    const mayFit = mode !== "REROUTE" && shouldFitCameraOnLateLayout({
      commanded,
      userMoved: next.userMoved,
      overlayOpen,
      cameraChanged: camerasDiffer(cameraAtStart, next.camera),
    });
    if (commanded) {
      useBoardStore.setState({
        userMoved: false,
        commandedLayout: mode === "COMPACT" ? "COMPACT" : "ARRANGE",
      });
    }
    if (mayFit) {
      const fitted = useBoardStore.getState();
      useBoardStore.getState().setCamera(fitBoardCamera(fitted, view));
    }
  } catch (error) {
    reportRoutingError(error);
    throw error;
  } finally {
    if (owns && layoutBusyOwner === token) {
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
  }).catch(reportRoutingError);
}

/** Native emits the new AST and script only after a successful graph compile. */
async function nativeGraphOp(payload: Record<string, string>): Promise<boolean> {
  try {
    const tx = beginPatchTx("graphOp", "canvas");
    const result = await getNativeFunction("graphOp")({
      origin: "canvas",
      txId: tx.id,
      baseRevision: tx.baseRevision,
      ...payload,
    }) as
      { ok?: boolean; error?: string; diagnostics?: Array<{line:number;column:number;message:string}> };
    if (result?.ok !== true) {
      useAstStore.setState({ diagnostics: result?.diagnostics?.length ? result.diagnostics :
        [{ line: 1, column: 1, message: result?.error || "Graph edit could not be compiled." }] });
      return false;
    }
    return true;
  } catch (error) {
    useAstStore.setState({ diagnostics: [{ line: 1, column: 1,
      message: error instanceof Error ? error.message : String(error) }] });
    return false;
  }
}

export async function commitBoardConnect(src: BoardPort, dst: BoardPort): Promise<void> {
  const from = graphOpEndpoints(src);
  const to = graphOpEndpoints(dst);
  if (from.nodeId === "MIDI" || to.nodeId === "MIDI") {
    return;
  }
  if (hasJuceBridge()) {
    await nativeGraphOp({ op: "connect", from: from.nodeId, fromJack: from.jackId, to: to.nodeId, toJack: to.jackId });
    return;
  }
  const cur = useAstStore.getState();
  try {
    const next = scriptAfterConnect(cur.lastValidScript || cur.script, from.nodeId, to.nodeId,
      { fromJack: from.jackId, toJack: to.jackId, kind: src.kind });
    await publishScript(next, "canvas");
    rerouteBoard();
  } catch (error) {
    useAstStore.setState({ diagnostics: [{ line: 1, column: 1,
      message: error instanceof Error ? error.message : "Connection could not be created." }] });
  }
}

export async function commitBoardCut(port: BoardPort): Promise<void> {
  const g = useBoardStore.getState();
  const doomed = Object.values(g.edges).filter((e) =>
    e.dependency !== "midi" && (e.sourcePortId === port.id || e.targetPortId === port.id));
  if (!doomed.length) return;
  if (hasJuceBridge()) {
    for (const e of doomed) {
      const src = g.ports[e.sourcePortId];
      const dst = g.ports[e.targetPortId];
      if (! src || ! dst || graphOpEndpoints(src).nodeId === "MIDI") {
        continue;
      }
      const from = graphOpEndpoints(src);
      const to = graphOpEndpoints(dst);
      if (!await nativeGraphOp({ op: "disconnect", from: from.nodeId,
        fromJack: from.jackId, to: to.nodeId, toJack: to.jackId })) break;
    }
    return;
  }
  useBoardStore.getState().setEdges(edgesAfterCutPort(g, port.id));
  const ast = useAstStore.getState().ast;
  if (ast) pushAst(astEdgesAfterCut(ast.edges ?? [], port));
  const cur = useAstStore.getState();
  let script = cur.lastValidScript || cur.script;
  for (const e of doomed) script = scriptAfterDisconnect(script, e.sourceNodeId, e.targetNodeId);
  if (script !== (cur.lastValidScript || cur.script)) await publishScript(script, "canvas");
  rerouteBoard();
}
