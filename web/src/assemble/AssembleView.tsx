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
  type EdgeChange,
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
import { SignalEdge } from "./SignalEdge";
import {
  alreadyLinked,
  directedProximity,
  findClosestChip,
  primaryJackId,
} from "./connectModel";
import { handleId } from "./handles";
import { keepLivePositions, mergeBoardNodes, nextSeenIds, shouldAutoArrange } from "./boardSync";
import { arrangeElk } from "./elkArrange";
import { flowFromAst, visibleNodes, type ChipData } from "./flowFromAst";
import { parseHandle } from "./handles";
import { routeBoard } from "./routeBoard";
import { isValidLink } from "./validateLink";
import { addCircuitBlock, removeCircuitBlock } from "./addBlock";

const nodeTypes = { chip: ChipNode, io: IoNode };
const edgeTypes = { signal: SignalEdge };

const defaultEdgeOptions = {
  type: "signal" as const,
  animated: false,
  style: { stroke: "#ff003c", strokeWidth: 20 },
};

function jackOf(node: Node<ChipData> | undefined, handle: string | null | undefined): { kind: string; output: boolean } | null {
  if (! node || ! handle) {
    return null;
  }
  const parsed = parseHandle(handle);
  if (! parsed) {
    return { kind: "audio", output: false };
  }
  const jack = node.data.jacks.find((j) => j.id === parsed.id);
  if (! jack) {
    return { kind: parsed.id === "mod" || parsed.id.startsWith("mod") ? "mod" : "audio", output: parsed.output };
  }
  return { kind: jack.kind, output: jack.output };
}

function Board() {
  const ast = useAstStore((s) => s.ast);
  const origin = useAstStore((s) => s.origin);
  const built = useMemo(() => (ast ? flowFromAst(ast) : { nodes: [], edges: [] }), [ast]);
  const [nodes, setNodes, onNodesChange] = useNodesState(built.nodes);
  const [edges, setEdges, onEdgesChange] = useEdgesState(built.edges);
  const [menu, setMenu] = useState<{ kind: "node" | "pane"; id: string; left: number; top: number } | null>(null);
  const paneRef = useRef<HTMLDivElement>(null);
  const prevIdsRef = useRef<string[]>([]);
  const store = useStoreApi();
  const { getInternalNode, fitView } = useReactFlow();
  const updateInternals = useUpdateNodeInternals();

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
    setNodes((prev) => mergeBoardNodes(prev, built.nodes, keepLive && prev.length > 0));
    built.nodes.forEach((n) => updateInternals(n.id));
    setEdges(built.edges);
    if (! auto) {
      return () => {
        cancel = true;
      };
    }
    const view = {
      w: paneRef.current?.clientWidth || 1200,
      h: paneRef.current?.clientHeight || 420,
    };
    void arrangeElk(built.nodes, built.edges.filter((e) => e.className !== "temp"), view).then((pos) => {
      if (cancel || Object.keys(pos).length === 0) {
        return;
      }
      prevIdsRef.current = nextSeenIds(prevIdsRef.current, nextIds, false);
      setNodes((ns) => ns.map((n) => (pos[n.id] ? { ...n, position: pos[n.id] } : n)));
      requestAnimationFrame(() => {
        if (! cancel) {
          void fitView({ padding: 0.1, duration: 0, minZoom: 0.75, maxZoom: 1 });
        }
      });
    });
    return () => {
      cancel = true;
    };
  }, [ast, built, origin, setEdges, setNodes, updateInternals]);

  const posKey = nodes.map((n) => `${n.id}:${n.position.x}:${n.position.y}:${n.height ?? 0}`).join("|");
  useEffect(() => {
    nodes.forEach((n) => updateInternals(n.id));
  }, [posKey, updateInternals, nodes]);
  const edgeKey = edges.map((e) => `${e.id}:${e.source}:${e.target}:${e.sourceHandle}:${e.targetHandle}`).join("|");
  useEffect(() => {
    const mapped = routeBoard(nodes, edges);
    setEdges((eds) => {
      let changed = false;
      const next = eds.map((e) => {
        const r = mapped.get(e.id);
        if (! r || (e.data as { route?: string } | undefined)?.route === r.d) {
          return e;
        }
        changed = true;
        return { ...e, data: { ...(e.data as object), route: r.d, points: r.points } };
      });
      return changed ? next : eds;
    });
  }, [posKey, edgeKey, edges, nodes, setEdges]);

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

  const commitConnect = useCallback((c: Connection) => {
    if (! isValidConnection(c)) {
      return;
    }
    setEdges((eds) => {
      if (alreadyLinked(eds, String(c.source), String(c.target))) {
        return eds;
      }
      return addEdge({ ...c, ...defaultEdgeOptions }, eds);
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
  }, [isValidConnection, setEdges]);

  const onConnect = useCallback((c: Connection) => {
    commitConnect(c);
  }, [commitConnect]);

  const closestPair = useCallback((node: Node) => {
    const internal = getInternalNode(node.id);
    if (! internal) {
      return null;
    }
    const others = Array.from(store.getState().nodeLookup.values())
      .filter((n) => n.id !== node.id)
      .map((n) => ({
        id: n.id,
        x: n.internals.positionAbsolute.x,
        y: n.internals.positionAbsolute.y,
      }));
    const hit = findClosestChip({
      id: node.id,
      x: internal.internals.positionAbsolute.x,
      y: internal.internals.positionAbsolute.y,
    }, others);
    if (! hit) {
      return null;
    }
    const dir = directedProximity(
      { id: node.id, x: internal.internals.positionAbsolute.x },
      hit,
    );
    const src = nodes.find((n) => n.id === dir.source);
    const dst = nodes.find((n) => n.id === dir.target);
    if (! src || ! dst) {
      return null;
    }
    const fromJack = primaryJackId(src.data.jacks, true);
    const toJack = primaryJackId(dst.data.jacks, false);
    const fromKind = src.data.jacks.find((j) => j.id === fromJack)?.kind ?? "audio";
    const toKind = dst.data.jacks.find((j) => j.id === toJack)?.kind ?? "audio";
    if (! isValidLink({ kind: fromKind, output: true }, { kind: toKind, output: false })) {
      return null;
    }
    return {
      source: dir.source,
      target: dir.target,
      sourceHandle: handleId(fromJack, true),
      targetHandle: handleId(toJack, false),
    };
  }, [getInternalNode, nodes, store]);

  const onNodeDrag = useCallback((_: unknown, node: Node) => {
    const pair = closestPair(node);
    setEdges((es) => {
      const next = es.filter((e) => e.className !== "temp");
      if (pair && ! alreadyLinked(next, pair.source, pair.target)) {
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
    if (! hasJuceBridge() || node.id === "IN") {
      return;
    }
    void getNativeFunction("applyLayout")({
      origin: "canvas",
      positions: { [node.id]: { x: node.position.x, y: node.position.y } },
    }).catch(() => undefined);
  }, [closestPair, commitConnect, setEdges]);

  const onNodeDoubleClick = useCallback((_: unknown, node: Node) => {
    if (node.id === "IN" || node.id === "OUT") {
      return;
    }
    useHostStore.getState().setOverlay("inspect", node.id);
  }, []);

  const arrange = useCallback(async () => {
    const view = {
      w: paneRef.current?.clientWidth || 1200,
      h: paneRef.current?.clientHeight || 420,
    };
    const pos = await arrangeElk(nodes, edges.filter((e) => e.className !== "temp"), view);
    setNodes((ns) => ns.map((n) => (pos[n.id] ? { ...n, position: pos[n.id] } : n)));
    requestAnimationFrame(() => {
      void fitView({ padding: 0.1, duration: 0, minZoom: 0.75, maxZoom: 1 });
    });
    if (hasJuceBridge()) {
      const chips: Record<string, { x: number; y: number }> = {};
      for (const [id, p] of Object.entries(pos)) {
        if (id !== "IN") {
          chips[id] = p;
        }
      }
      void getNativeFunction("applyLayout")({ origin: "elk", positions: chips }).catch(() => undefined);
    }
  }, [edges, fitView, nodes, setNodes]);

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
      if (e.key !== "a" || ! (e.metaKey || e.ctrlKey)) {
        return;
      }
      e.preventDefault();
      void arrange();
    };
    window.addEventListener("keydown", onKey);
    return () => window.removeEventListener("keydown", onKey);
  }, [arrange]);

  return (
    <div ref={paneRef} className="relative h-full min-h-0 w-full bg-black">
      <ReactFlow
        className="nk-flow"
        nodes={nodes}
        edges={edges}
        onNodesChange={onNodesChange}
        onEdgesChange={(changes: EdgeChange[]) => {
          onEdgesChange(changes.filter((c) => c.type !== "remove" || ! String(c.id).startsWith("e-")));
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
        onNodeContextMenu={onNodeContextMenu}
        onPaneContextMenu={onPaneContextMenu}
        onPaneClick={closeMenu}
        isValidConnection={isValidConnection}
        nodeTypes={nodeTypes}
        edgeTypes={edgeTypes}
        connectionLineComponent={ConnectionLine}
        connectionMode={ConnectionMode.Loose}
        defaultEdgeOptions={defaultEdgeOptions}
        defaultViewport={{ x: 24, y: 24, zoom: 1 }}
        minZoom={0.4}
        maxZoom={2}
        nodesDraggable
        nodesConnectable
        nodeDragThreshold={1}
        deleteKeyCode={["Backspace", "Delete"]}
        onlyRenderVisibleElements
        zoomOnDoubleClick={false}
        proOptions={{ hideAttribution: true }}
      >
        <Background variant={BackgroundVariant.Lines} color="rgba(255,0,60,0.10)" gap={32} />
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
