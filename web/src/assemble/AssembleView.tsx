import {
  Background,
  BackgroundVariant,
  ConnectionMode,
  ReactFlow,
  ReactFlowProvider,
  addEdge,
  useEdgesState,
  useNodesState,
  useReactFlow,
  useStoreApi,
  useUpdateNodeInternals,
  type Connection,
  type Edge,
  type Node,
} from "@xyflow/react";
import "@xyflow/react/dist/style.css";
import { useCallback, useEffect, useMemo, useRef, useState, type MouseEvent } from "react";
import { getNativeFunction, hasJuceBridge } from "../bridge/juce";
import { useAstStore } from "../store/astStore";
import { useHostStore } from "../store/hostStore";
import { menuPos } from "../theme/fit";
import { OsAddPicker, OsContextMenu, OsMenuItem } from "../overlays/OsContextMenu";
import { addPickerSize } from "../overlays/addPicker";
import { useChipViewStore } from "../store/expandStore";
import { ChipNode, IoNode } from "./ChipNode";
import { ConnectionLine } from "./ConnectionLine";
import { StaticGridEdge } from "./StaticGridEdge";
import {
  alreadyLinked,
  dndCableEndChrome,
  dndNodeChrome,
  findClosestJack,
  proximityPairFromJacks,
  scriptAfterDisconnect,
  type JackPoint,
} from "./connectModel";
import { circuitDofAllowed, focusAttr, focusPlane } from "./circuitDof";
import { keepLivePositions, mergeBoardNodes, nextSeenIds, shouldAutoArrange } from "./boardSync";
import { jackTopPx } from "./chipLayout";
import { applyLayoutResult, requestLayout } from "./layout/layoutClient";
import type { LayoutMode } from "./layout/types";
import { flowFromAst, visibleNodes, type ChipData } from "./flowFromAst";
import { parseHandle } from "./handles";
import { isValidLink } from "./validateLink";
import { addCircuitBlock, publishScript, removeCircuitBlock } from "./addBlock";
import { chipOverlay, isIrSlotId } from "../presets/irSlots";
import { openImpulse } from "../overlays/ImpulsePanel";

import { BOARD_BLOCK, BOARD_DOT, BOARD_GRID, BOARD_TRACE } from "./grid";

const nodeTypes = { chip: ChipNode, io: IoNode };
const edgeTypes = { signal: StaticGridEdge };
const cableEndChrome = dndCableEndChrome();
const nodeChrome = dndNodeChrome({ locked: false });

const defaultEdgeOptions = {
  type: "signal" as const,
  animated: false,
  className: cableEndChrome.className,
  interactionWidth: 28,
  style: { stroke: "var(--nk-accent)", strokeWidth: BOARD_TRACE, cursor: cableEndChrome.cursor },
};

function jackOf(node: Node<ChipData> | undefined, handle: string | null | undefined): { kind: string; output: boolean; jack: string } | null {
  if (! node || ! handle) {
    return null;
  }
  const parsed = parseHandle(handle);
  if (! parsed) {
    return { kind: "audio", output: false, jack: "" };
  }
  const jack = node.data.jacks.find((j) => j.id === parsed.id);
  if (! jack) {
    return { kind: parsed.id === "mod" || parsed.id.startsWith("mod") ? "mod" : "audio", output: parsed.output, jack: parsed.id };
  }
  return { kind: jack.kind, output: jack.output, jack: jack.id };
}

function Board() {
  const ast = useAstStore((s) => s.ast);
  const origin = useAstStore((s) => s.origin);
  const motion = useHostStore((s) => s.motion);
  const sidechainOn = useHostStore((s) => s.sidechainOn);
  const built = useMemo(
    () => (ast ? flowFromAst(ast, { sidechainOn }) : { nodes: [], edges: [] }),
    [ast, sidechainOn],
  );
  const [nodes, setNodes, onNodesChange] = useNodesState([] as Node<ChipData>[]);
  const [edges, setEdges, onEdgesChange] = useEdgesState([] as Edge[]);
  const [menu, setMenu] = useState<{ kind: "node" | "pane"; id: string; left: number; top: number } | null>(null);
  const [hoverNodeId, setHoverNodeId] = useState<string | null>(null);
  const paneRef = useRef<HTMLDivElement>(null);
  const prevIdsRef = useRef<string[]>([]);
  const layoutGen = useRef(0);
  const nodesRef = useRef(nodes);
  const edgesRef = useRef(edges);
  nodesRef.current = nodes;
  edgesRef.current = edges;
  const store = useStoreApi();
  const { getInternalNode, fitView } = useReactFlow();
  const updateInternals = useUpdateNodeInternals();
  const prefersReduced = typeof window !== "undefined"
    && window.matchMedia?.("(prefers-reduced-motion: reduce)").matches === true;
  const dofAllowed = circuitDofAllowed(motion, prefersReduced);
  const plane = useMemo(() => focusPlane({
    selectedNodeIds: nodes.filter((n) => n.selected).map((n) => n.id),
    selectedEdgeIds: edges.filter((e) => e.selected && e.className !== "temp").map((e) => e.id),
    hoverNodeId,
    edges: edges
      .filter((e) => e.className !== "temp")
      .map((e) => ({ id: e.id, source: String(e.source), target: String(e.target) })),
  }), [edges, hoverNodeId, nodes]);
  const dofOn = dofAllowed && plane.active;
  const displayNodes = useMemo(() => nodes.map((n) => ({
    ...n,
    data: { ...n.data, focus: focusAttr(dofAllowed, plane, n.id) },
  })), [dofAllowed, nodes, plane]);
  const displayEdges = useMemo(() => edges.map((e) => ({
    ...e,
    data: { ...(e.data ?? {}), focus: focusAttr(dofAllowed, plane, e.id, "edge") },
  })), [dofAllowed, edges, plane]);

  useEffect(() => {
    let cancel = false;
    const nextIds = built.nodes.map((n) => n.id);
    const chipsHavePositions = ast
      ? visibleNodes(ast).every((n) => Number.isFinite(n.x) && Number.isFinite(n.y))
      : false;
    const auto = shouldAutoArrange({
      origin,
      prevIds: prevIdsRef.current,
      nextIds,
      chipsHavePositions,
    });
    const keepLive = keepLivePositions(origin);
    prevIdsRef.current = nextSeenIds(prevIdsRef.current, nextIds, auto);
    const merged = mergeBoardNodes(nodesRef.current, built.nodes, keepLive && nodesRef.current.length > 0);
    const liveEdges = built.edges.filter((e) => e.className !== "temp");
    const gen = layoutGen.current + 1;
    layoutGen.current = gen;
    const mode: LayoutMode = auto ? "ARRANGE" : "REROUTE";
    const view = {
      w: paneRef.current?.clientWidth || 960,
      h: paneRef.current?.clientHeight || 420,
    };
    void requestLayout(mode, auto ? built.nodes : merged, liveEdges, view).then((laid) => {
      if (cancel || gen !== layoutGen.current) {
        return;
      }
      if (auto) {
        prevIdsRef.current = nextSeenIds(prevIdsRef.current, nextIds, false);
      }
      const next = applyLayoutResult(laid, auto ? built.nodes : merged, liveEdges, auto);
      setNodes(next.nodes);
      setEdges(next.edges);
      next.nodes.forEach((n) => updateInternals(n.id));
      requestAnimationFrame(() => {
        if (! cancel && gen === layoutGen.current && (paneRef.current?.clientWidth ?? 0) > 40) {
          void fitView({ padding: 0.1, duration: 0, minZoom: 0.4, maxZoom: 1 });
        }
      });
    });
    return () => {
      cancel = true;
    };
  }, [ast, built, origin, fitView, setEdges, setNodes, updateInternals]);

  const posKey = nodes.map((n) => `${n.id}:${n.position.x}:${n.position.y}:${n.height ?? 0}`).join("|");
  useEffect(() => {
    nodes.forEach((n) => updateInternals(n.id));
  }, [posKey, updateInternals, nodes]);

  const isValidConnection = useCallback((c: Connection | Edge) => {
    const src = nodes.find((n) => n.id === c.source);
    const dst = nodes.find((n) => n.id === c.target);
    const a = jackOf(src, c.sourceHandle);
    const b = jackOf(dst, c.targetHandle);
    if (! a || ! b) {
      return false;
    }
    return isValidLink(a, b);
  }, [nodes]);

  const runBoardLayout = useCallback(async (
    mode: LayoutMode,
    moveNodes: boolean,
    ns = nodesRef.current,
    es = edgesRef.current,
  ) => {
    const gen = layoutGen.current + 1;
    layoutGen.current = gen;
    const live = es.filter((e) => e.className !== "temp");
    const view = {
      w: paneRef.current?.clientWidth || 960,
      h: paneRef.current?.clientHeight || 420,
    };
    const laid = await requestLayout(mode, ns, live, view);
    if (gen !== layoutGen.current) {
      return laid;
    }
    setNodes((cur) => applyLayoutResult(laid, cur, [], moveNodes).nodes);
    setEdges((cur) => applyLayoutResult(laid, [], cur, false).edges);
    if (moveNodes) {
      requestAnimationFrame(() => {
        void fitView({ padding: 0.1, duration: 0, minZoom: 0.4, maxZoom: 1 });
      });
    }
    return laid;
  }, [fitView, setEdges, setNodes]);

  const commitConnect = useCallback((c: Connection) => {
    if (! isValidConnection(c)) {
      return;
    }
    setEdges((eds) => {
      if (alreadyLinked(eds, String(c.source), String(c.target))) {
        return eds;
      }
      const next = addEdge({ ...c, ...defaultEdgeOptions }, eds);
      void runBoardLayout("REROUTE", false, nodesRef.current, next);
      return next;
    });
    if (hasJuceBridge()) {
      const fromJack = parseHandle(c.sourceHandle)?.id ?? "out";
      const toJack = parseHandle(c.targetHandle)?.id ?? "in";
      void getNativeFunction("graphOp")({
        origin: "canvas",
        op: "connect",
        from: c.source,
        fromJack,
        to: c.target,
        toJack,
      }).catch(() => undefined);
    }
  }, [isValidConnection, runBoardLayout, setEdges]);

  const onConnect = useCallback((c: Connection) => {
    commitConnect(c);
  }, [commitConnect]);

  const jackPointsOf = useCallback((nodeId: string | null = null): JackPoint[] => {
    const list: JackPoint[] = [];
    for (const n of store.getState().nodeLookup.values()) {
      if (nodeId && n.id !== nodeId) {
        continue;
      }
      const data = (nodes.find((x) => x.id === n.id)?.data ?? n.data) as ChipData | undefined;
      if (! data?.jacks) {
        continue;
      }
      const io = n.type === "io";
      const w = n.measured.width ?? n.width ?? (io ? 96 : 220);
      const h = n.measured.height ?? n.height ?? (io ? 56 : 80);
      const pos = n.internals.positionAbsolute;
      for (const j of data.jacks) {
        if (j.kind === "knob") {
          continue;
        }
        const side = data.jacks.filter((x) => x.output === j.output && x.kind !== "knob");
        const index = Math.max(0, side.findIndex((x) => x.id === j.id));
        list.push({
          nodeId: n.id,
          jackId: j.id,
          output: j.output,
          kind: j.kind,
          x: j.output ? pos.x + w : pos.x,
          y: pos.y + jackTopPx(index, Math.max(side.length, 1), h),
        });
      }
    }
    return list;
  }, [nodes, store]);

  const closestPair = useCallback((node: Node) => {
    if (! getInternalNode(node.id)) {
      return null;
    }
    const mine = jackPointsOf(node.id);
    const others = jackPointsOf().filter((j) => j.nodeId !== node.id);
    let best: { pair: NonNullable<ReturnType<typeof proximityPairFromJacks>>; distance: number } | null = null;
    for (const a of mine) {
      const hit = findClosestJack({ x: a.x, y: a.y, nodeId: a.nodeId }, others);
      if (! hit) {
        continue;
      }
      const pair = proximityPairFromJacks(a, hit);
      if (! pair) {
        continue;
      }
      const srcJack = a.output ? a : hit;
      const dstJack = a.output ? hit : a;
      if (! isValidLink(
        { kind: srcJack.kind, output: true, jack: srcJack.jackId },
        { kind: dstJack.kind, output: false, jack: dstJack.jackId },
      )) {
        continue;
      }
      if (alreadyLinked(edges, pair.source, pair.target, pair.sourceHandle, pair.targetHandle)) {
        continue;
      }
      if (! best || hit.distance < best.distance) {
        best = { pair, distance: hit.distance };
      }
    }
    return best?.pair ?? null;
  }, [edges, getInternalNode, jackPointsOf]);

  const persistDisconnect = useCallback((edge: Edge) => {
    if (edge.className === "temp") {
      return;
    }
    const from = String(edge.source);
    const to = String(edge.target);
    const fromJack = parseHandle(edge.sourceHandle)?.id ?? "out";
    const toJack = parseHandle(edge.targetHandle)?.id ?? "in";
    if (hasJuceBridge()) {
      void getNativeFunction("graphOp")({
        origin: "canvas",
        op: "disconnect",
        from,
        fromJack,
        to,
        toJack,
      }).catch(() => undefined);
      return;
    }
    const cur = useAstStore.getState();
    const script = cur.lastValidScript || cur.script;
    publishScript(scriptAfterDisconnect(script, from, to), "canvas");
  }, []);

  const onNodeDrag = useCallback((_: unknown, node: Node) => {
    const pair = closestPair(node);
    setEdges((es) => {
      const next = es.filter((e) => e.className !== "temp");
      if (pair && ! alreadyLinked(next, pair.source, pair.target, pair.sourceHandle, pair.targetHandle)) {
        next.push({
          id: `temp-${pair.source}-${pair.target}`,
          ...pair,
          ...defaultEdgeOptions,
          className: "temp",
          data: { temp: true },
        });
      }
      return next;
    });
  }, [closestPair, setEdges]);

  const onNodeDragStop = useCallback((_: unknown, node: Node) => {
    const pair = closestPair(node);
    setEdges((es) => es.filter((e) => e.className !== "temp"));
    if (pair) {
      commitConnect(pair);
    }
    void runBoardLayout("REROUTE", false);
    if (! hasJuceBridge() || node.id === "IN") {
      return;
    }
    void getNativeFunction("applyLayout")({
      origin: "canvas",
      positions: { [node.id]: { x: node.position.x, y: node.position.y } },
    }).catch(() => undefined);
  }, [closestPair, commitConnect, runBoardLayout, setEdges]);

  const onNodeDoubleClick = useCallback((_: unknown, node: Node) => {
    if (node.id === "IN" || node.id === "OUT") {
      return;
    }
    const type = String((node.data as { type?: string } | undefined)?.type ?? node.id);
    const hit = chipOverlay(node.id, type);
    useHostStore.getState().setOverlay(hit.overlay, hit.inspectId);
  }, []);

  const persistPositions = useCallback((laid: Awaited<ReturnType<typeof requestLayout>>) => {
    if (! hasJuceBridge()) {
      return;
    }
    const chips: Record<string, { x: number; y: number }> = {};
    for (const [id, p] of Object.entries(laid.nodes)) {
      if (id !== "IN") {
        chips[id] = { x: p.x, y: p.y };
      }
    }
    void getNativeFunction("applyLayout")({ origin: "elk", positions: chips }).catch(() => undefined);
  }, []);

  const arrange = useCallback(async () => {
    const laid = await runBoardLayout("ARRANGE", true);
    persistPositions(laid);
  }, [persistPositions, runBoardLayout]);

  const compactBoard = useCallback(async () => {
    const laid = await runBoardLayout("COMPACT", true);
    persistPositions(laid);
  }, [persistPositions, runBoardLayout]);

  const onNodeContextMenu = useCallback((event: MouseEvent, node: Node) => {
    event.preventDefault();
    const pane = paneRef.current;
    if (! pane) {
      return;
    }
    setMenu({ kind: "node", id: node.id, ...menuPos(event.clientX, event.clientY, pane) });
  }, []);

  const onPaneContextMenu = useCallback((event: MouseEvent | { preventDefault: () => void; clientX: number; clientY: number }) => {
    event.preventDefault();
    const pane = paneRef.current;
    if (! pane) {
      return;
    }
    const box = addPickerSize();
    setMenu({ kind: "pane", id: "board", ...menuPos(event.clientX, event.clientY, pane, box.w, box.h + 36) });
  }, []);

  const closeMenu = useCallback(() => setMenu(null), []);

  useEffect(() => {
    const onKey = (e: KeyboardEvent) => {
      const t = e.target as HTMLElement | null;
      if (t && (t.tagName === "INPUT" || t.tagName === "TEXTAREA" || t.isContentEditable)) {
        return;
      }
      if (e.key.toLowerCase() !== "a") {
        return;
      }
      if (e.metaKey || e.ctrlKey) {
        e.preventDefault();
        void arrange();
        return;
      }
      if (e.shiftKey) {
        e.preventDefault();
        void compactBoard();
      }
    };
    window.addEventListener("keydown", onKey);
    return () => window.removeEventListener("keydown", onKey);
  }, [arrange, compactBoard]);

  return (
    <div
      ref={paneRef}
      className="nk-circuit relative h-full min-h-0 w-full overflow-hidden bg-black"
      style={{
        cursor: nodeChrome.cursor,
        ["--nk-grid" as string]: `${BOARD_GRID}px`,
        ["--nk-trace" as string]: `${BOARD_TRACE}px`,
      }}
      data-dof={dofOn ? "on" : "off"}
      data-focus={dofOn ? "plane" : "none"}
    >
      <ReactFlow
        className={`nk-flow ${nodeChrome.className}`}
        nodes={displayNodes}
        edges={displayEdges}
        onNodesChange={onNodesChange}
        onEdgesChange={onEdgesChange}
        onBeforeDelete={async ({ edges: doomed }) => ({
          nodes: [],
          edges: doomed.filter((e) => e.className !== "temp"),
        })}
        onEdgesDelete={(deleted) => {
          for (const e of deleted) {
            persistDisconnect(e);
          }
          const gone = new Set(deleted.map((e) => e.id));
          void runBoardLayout(
            "REROUTE",
            false,
            nodesRef.current,
            edgesRef.current.filter((e) => ! gone.has(e.id)),
          );
        }}
        onError={(code, msg) => {
          if (code === "008" && import.meta.env.DEV) {
            console.warn(msg);
          }
        }}
        onConnect={onConnect}
        onNodeDrag={onNodeDrag}
        onNodeDragStop={onNodeDragStop}
        onNodeDoubleClick={onNodeDoubleClick}
        onNodeMouseEnter={(_, node) => setHoverNodeId(node.id)}
        onNodeMouseLeave={() => setHoverNodeId(null)}
        onNodeContextMenu={onNodeContextMenu}
        onPaneContextMenu={onPaneContextMenu}
        onPaneClick={() => {
          setHoverNodeId(null);
          closeMenu();
        }}
        isValidConnection={isValidConnection}
        nodeTypes={nodeTypes}
        edgeTypes={edgeTypes}
        connectionLineComponent={ConnectionLine}
        connectionMode={ConnectionMode.Loose}
        defaultEdgeOptions={defaultEdgeOptions}
        defaultViewport={{ x: BOARD_GRID, y: BOARD_GRID, zoom: 1 }}
        minZoom={0.4}
        maxZoom={2}
        snapToGrid
        snapGrid={[BOARD_GRID, BOARD_GRID]}
        nodesDraggable
        nodesConnectable
        edgesReconnectable
        nodeDragThreshold={1}
        deleteKeyCode={["Backspace", "Delete"]}
        onlyRenderVisibleElements
        zoomOnDoubleClick={false}
        proOptions={{ hideAttribution: true }}
      >
        <Background
          id="nk-cell"
          variant={BackgroundVariant.Dots}
          gap={BOARD_GRID}
          size={BOARD_DOT}
          offset={BOARD_GRID / 2}
          color="rgba(var(--nk-accent-rgb), 0.42)"
        />
        <Background
          id="nk-block"
          variant={BackgroundVariant.Lines}
          gap={BOARD_BLOCK}
          color="rgba(var(--nk-accent-rgb), 0.10)"
        />
      </ReactFlow>

      {menu ? (
        <OsContextMenu left={menu.left} top={menu.top} title={menu.kind === "node" ? menu.id : "NKOS // BOARD"} onDismiss={closeMenu}>
          {menu.kind === "node" ? (
            <OsMenuItem onClick={() => {
              useChipViewStore.getState().toggle(menu.id);
              closeMenu();
            }}>Details</OsMenuItem>
          ) : null}
          {menu.kind === "node" && menu.id !== "IN" && menu.id !== "OUT" ? (
            <OsMenuItem onClick={() => { useHostStore.getState().setOverlay("inspect", menu.id); closeMenu(); }}>Inspect</OsMenuItem>
          ) : null}
          {menu.kind === "node" && isIrSlotId(menu.id) ? (
            <OsMenuItem onClick={() => { openImpulse(menu.id); closeMenu(); }}>Load cab</OsMenuItem>
          ) : null}
          {menu.kind === "node" && menu.id !== "IN" && menu.id !== "OUT" ? (
            <OsMenuItem onClick={() => {
              removeCircuitBlock(menu.id);
              closeMenu();
            }}>Delete</OsMenuItem>
          ) : null}
          {menu.kind === "pane" ? (
            <OsAddPicker
              onPick={(type, args) => {
                addCircuitBlock(type, args);
                closeMenu();
              }}
            />
          ) : null}
          <OsMenuItem onClick={() => { void arrange(); closeMenu(); }}>Arrange</OsMenuItem>
          <OsMenuItem onClick={() => { void compactBoard(); closeMenu(); }}>Compact</OsMenuItem>
        </OsContextMenu>
      ) : null}
    </div>
  );
}

export function AssembleView() {
  return (
    <ReactFlowProvider>
      <Board />
    </ReactFlowProvider>
  );
}
