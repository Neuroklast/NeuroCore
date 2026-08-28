import { useCallback, useEffect, useLayoutEffect, useMemo, useRef, useState, type PointerEvent, type WheelEvent } from "react";
import { hasJuceBridge } from "../bridge/juce";
import { useAstStore } from "../store/astStore";
import { parseClipPeaks, useHostStore } from "../store/hostStore";
import { subscribeVizClock } from "../theme/vizClock";
import { shouldCollapseChipDetail, useChipViewStore } from "../store/expandStore";
import { useBindStore } from "../store/telemetryStore";
import { OsAddPicker, OsContextMenu, OsMenuItem } from "../overlays/OsContextMenu";
import { isArrangeChord } from "../chrome/shortcuts";
import { chipOverlay } from "../presets/irSlots";
import { addCircuitBlock, insertCircuitBlockAfter, removeCircuitBlock } from "./addBlock";
import { BoardChip } from "./BoardChip";
import { applyCameraTransform, cameraMatrix, fitCamera, panCamera, worldFromScreen, zoomCamera } from "./boardCamera";
import { commitBoardConnect, commitBoardCut, layoutBoard, rerouteBoard } from "./boardCommit";
import { connectDragRef, magnetPort } from "./boardConnect";
import { boardContextHit, canDeleteChip, chipAtWorld, circuitAllowsTextSelect, hitBoardEdge } from "./boardEdit";
import { graphToLayout, portGlobal, previewBoardIds, type BoardPort } from "./boardModel";
import { applyChipDragStyle, chipDragRef, type ChipDrag } from "./boardDrag";
import { useBoardStore } from "./boardStore";
import { keepBoardXy } from "./boardSync";
import { CableCanvas, paintCablesNow } from "./CableCanvas";
import { demoClipRows } from "./demoClips";
import { applyBoardFocus, boardFocusEdgesRef, boardHoverRef, circuitDofAllowed, focusAttr, focusPlane } from "./circuitDof";
import { CHIP_AIR_X, CHIP_AIR_Y, snapToGrid } from "./grid";
import { serialIds, wrapFits } from "./layout/compactPack";

type BoardMenu =
  | { kind: "pane"; left: number; top: number; world: { x: number; y: number } }
  | { kind: "chip"; left: number; top: number; id: string }
  | { kind: "add"; left: number; top: number; afterId: string };

function inspectChip(id: string, type: string): void {
  const o = chipOverlay(id, type);
  useHostStore.getState().setOverlay(o.overlay, o.inspectId);
}

export function BoardView({ active = true }: { active?: boolean }) {
  const ast = useAstStore((s) => s.ast);
  const origin = useAstStore((s) => s.origin);
  const sidechainOn = useHostStore((s) => s.sidechainOn);
  const nodes = useBoardStore((s) => s.nodes);
  const ports = useBoardStore((s) => s.ports);
  const edges = useBoardStore((s) => s.edges);
  const userMoved = useBoardStore((s) => s.userMoved);
  const paneRef = useRef<HTMLDivElement>(null);
  const worldRef = useRef<HTMLDivElement>(null);
  const camLive = useRef(useBoardStore.getState().camera);
  const camGesture = useRef(false);
  const [size, setSize] = useState({ w: 960, h: 420 });
  const panRef = useRef<{ x: number; y: number; tx: number; ty: number } | null>(null);
  const dragRef = useRef<{ id: string; dx: number; dy: number; originX: number; originY: number } | null>(null);
  const spaceRef = useRef(false);
  const [selected, setSelected] = useState<string | null>(null);
  const selectedRef = useRef<string | null>(null);
  selectedRef.current = selected;
  const [menu, setMenu] = useState<BoardMenu | null>(null);
  const placeRef = useRef<{ x: number; y: number } | { afterId: string } | null>(null);
  const idsRef = useRef<Set<string>>(new Set());
  const bindLetter = useBindStore((s) => s.letter);
  const bindX = useBindStore((s) => s.x);
  const bindY = useBindStore((s) => s.y);
  const motion = useHostStore((s) => s.motion);
  const prefersReduced = typeof window !== "undefined"
    && window.matchMedia?.("(prefers-reduced-motion: reduce)").matches === true;
  const dofAllowed = circuitDofAllowed(motion, prefersReduced);

  const paintFocus = useCallback(() => {
    const pane = paneRef.current;
    const g = useBoardStore.getState();
    const plane = focusPlane({
      selectedNodeIds: selectedRef.current ? [selectedRef.current] : [],
      selectedEdgeIds: [],
      hoverNodeId: boardHoverRef.current,
      edges: Object.values(g.edges).map((e) => ({
        id: e.id,
        source: e.sourceNodeId,
        target: e.targetNodeId,
      })),
    });
    boardFocusEdgesRef.current = dofAllowed && plane.active ? plane.edges : null;
    if (pane) {
      applyBoardFocus(pane.querySelectorAll("[data-node-id]"), (id) => focusAttr(dofAllowed, plane, id));
    }
    paintCablesNow();
  }, [dofAllowed]);

  useEffect(() => {
    const prevIds = Object.keys(useBoardStore.getState().nodes);
    const keep = keepBoardXy({
      origin,
      prevIds,
      nextIds: previewBoardIds(ast, sidechainOn),
      chipsHavePositions: true,
    });
    useBoardStore.getState().hydrate(ast, sidechainOn, keep);
    if (origin === "preset") {
      useChipViewStore.getState().collapseAll();
    }
  }, [ast, origin, sidechainOn]);

  useEffect(() => {
    if (hasJuceBridge()) {
      return;
    }
    return subscribeVizClock((now) => {
      const list = Object.values(useBoardStore.getState().nodes)
        .sort((a, b) => a.x - b.x || a.y - b.y)
        .map((n) => n.id);
      const parsed = parseClipPeaks(demoClipRows(list, now / 1000));
      const hold = (window as unknown as { __nkClipHold?: Record<string, number> }).__nkClipHold;
      if (hold) {
        for (const [id, p] of Object.entries(hold)) {
          parsed.peak[id] = p;
          parsed.peakL[id] = p;
          parsed.peakR[id] = p;
          const e = Math.min(1, p) * Math.SQRT1_2;
          parsed.rms[id] = e;
          parsed.rmsL[id] = e;
          parsed.rmsR[id] = e;
        }
      }
      useHostStore.setState({
        clips: parsed.peak,
        clipsL: parsed.peakL,
        clipsR: parsed.peakR,
        clipsRms: parsed.rms,
        clipsRmsL: parsed.rmsL,
        clipsRmsR: parsed.rmsR,
      });
    });
  }, []);

  useEffect(() => {
    const place = placeRef.current;
    const ids = new Set(Object.keys(nodes));
    if (place) {
      const fresh = [...ids].filter((id) => ! idsRef.current.has(id) && nodes[id]?.role === "chip");
      const n = fresh[0] ? nodes[fresh[0]] : undefined;
      if (n) {
        if ("afterId" in place) {
          const after = nodes[place.afterId] ?? nodes.IN;
          if (after) {
            useBoardStore.getState().moveNode(
              n.id,
              snapToGrid(after.x + after.w + CHIP_AIR_X),
              snapToGrid(after.y),
            );
          }
        } else {
          useBoardStore.getState().moveNode(n.id, snapToGrid(place.x - n.w * 0.5), snapToGrid(place.y - n.h * 0.5));
        }
      }
      placeRef.current = null;
    }
    idsRef.current = ids;
  }, [nodes]);

  useEffect(() => {
    const el = paneRef.current;
    if (! el) {
      return;
    }
    const ro = new ResizeObserver(() => {
      setSize({ w: el.clientWidth, h: el.clientHeight });
    });
    ro.observe(el);
    setSize({ w: el.clientWidth, h: el.clientHeight });
    const blockSelect = (e: Event) => {
      const tag = (e.target as HTMLElement | null)?.tagName;
      if (! circuitAllowsTextSelect(tag === "INPUT" || tag === "TEXTAREA" ? "field" : "pane")) {
        e.preventDefault();
      }
    };
    el.addEventListener("selectstart", blockSelect);
    return () => {
      ro.disconnect();
      el.removeEventListener("selectstart", blockSelect);
    };
  }, []);

  useEffect(() => {
    if (userMoved || size.w < 40) {
      return;
    }
    const ns = Object.values(nodes);
    if (ns.length === 0) {
      return;
    }
    useBoardStore.getState().setCamera(fitCamera(ns, size));
  }, [nodes, userMoved, size.w, size.h]);

  const applyLiveCamera = useCallback((cam: typeof camLive.current, commit = false) => {
    camLive.current = cam;
    if (worldRef.current) {
      applyCameraTransform(worldRef.current, cam);
    }
    paintCablesNow();
    if (commit) {
      useBoardStore.getState().setCamera(cam);
    }
  }, []);

  useEffect(() => {
    const apply = (cam: typeof camLive.current) => {
      if (camGesture.current) {
        return;
      }
      applyLiveCamera(cam);
    };
    apply(useBoardStore.getState().camera);
    return useBoardStore.subscribe((s, prev) => {
      if (s.camera === prev.camera) {
        return;
      }
      apply(s.camera);
    });
  }, [applyLiveCamera]);

  useLayoutEffect(() => {
    paintFocus();
  }, [selected, nodes, paintFocus]);

  useEffect(() => {
    if (userMoved || Object.keys(nodes).length === 0) {
      return;
    }
    if (origin === "canvas") {
      return;
    }
    let cancel = false;
    const g = useBoardStore.getState();
    const payload = graphToLayout(g);
    const view = { w: size.w || 960, h: size.h || 420 };
    const order = serialIds(payload.nodes, payload.edges, "IN");
    const per = wrapFits(order.map((id) => g.nodes[id]?.w ?? 32), view.w, CHIP_AIR_X, CHIP_AIR_Y);
    const mode = per < order.length ? "COMPACT" : "ARRANGE";
    void layoutBoard(mode, view).then(() => {
      if (cancel) {
        return;
      }
    });
    return () => {
      cancel = true;
    };
  }, [ast, origin, userMoved, size.w, size.h, Object.keys(nodes).length]);

  const onPointerDown = useCallback((e: PointerEvent<HTMLDivElement>) => {
    const pane = paneRef.current;
    if (! pane) {
      return;
    }
    const rect = pane.getBoundingClientRect();
    const cam = camLive.current;
    const world = worldFromScreen(cam, e.clientX - rect.left, e.clientY - rect.top);
    const portEl = (e.target as HTMLElement).closest("[data-port-id]");
    const portIdHit = portEl?.getAttribute("data-port-id");
    if (portIdHit) {
      const g = useBoardStore.getState();
      const port = g.ports[portIdHit] as BoardPort | undefined;
      const node = port ? g.nodes[port.nodeId] : undefined;
      if (port && node) {
        e.stopPropagation();
        if (e.altKey) {
          commitBoardCut(port);
          return;
        }
        if (! port.east) {
          return;
        }
        const from = portGlobal(node, port);
        connectDragRef.current = {
          fromPort: port,
          from,
          to: from,
          snapPortId: null,
          kind: port.kind,
        };
        pane.setPointerCapture(e.pointerId);
        return;
      }
    }
    if (e.button === 1 || spaceRef.current || e.button === 0 && !(e.target as HTMLElement).closest("[data-node-id], [data-port-id]")) {
      const edge = hitBoardEdge(world, useBoardStore.getState());
      if (e.button === 0 && edge) {
        setMenu({
          kind: "add",
          left: e.clientX - rect.left,
          top: e.clientY - rect.top,
          afterId: edge.sourceNodeId,
        });
        return;
      }
      panRef.current = { x: e.clientX, y: e.clientY, tx: cam.tx, ty: cam.ty };
      camGesture.current = true;
      pane.setPointerCapture(e.pointerId);
      return;
    }
    if (e.button !== 0) {
      return;
    }
    const chip = (e.target as HTMLElement).closest("[data-node-id]");
    const id = chip?.getAttribute("data-node-id");
    if (! id || (e.target as HTMLElement).closest(".nk-port, .nk-chip-expand, .nk-ms, .nk-chip-overlay, .nk-bind-jack, .nk-bind-cell")) {
      return;
    }
    const n = useBoardStore.getState().nodes[id];
    if (! n || n.locked) {
      setSelected(id);
      return;
    }
    dragRef.current = { id, dx: world.x - n.x, dy: world.y - n.y, originX: n.x, originY: n.y };
    setSelected(id);
    pane.setPointerCapture(e.pointerId);
  }, []);

  const onPointerMove = useCallback((e: PointerEvent<HTMLDivElement>) => {
    const pane = paneRef.current;
    if (! pane) {
      return;
    }
    const cam = camLive.current;
    if (connectDragRef.current) {
      const rect = pane.getBoundingClientRect();
      const world = worldFromScreen(cam, e.clientX - rect.left, e.clientY - rect.top);
      const g = useBoardStore.getState();
      const snap = magnetPort(world, connectDragRef.current.fromPort, g);
      const to = snap && g.nodes[snap.nodeId]
        ? portGlobal(g.nodes[snap.nodeId]!, snap)
        : world;
      connectDragRef.current = {
        ...connectDragRef.current,
        to,
        snapPortId: snap?.id ?? null,
      };
      pane.querySelectorAll(".nk-port-hot").forEach((el) => el.classList.remove("nk-port-hot"));
      if (snap) {
        pane.querySelector(`[data-port-id="${snap.id}"]`)?.classList.add("nk-port-hot");
      }
      paintCablesNow();
      return;
    }
    if (panRef.current) {
      applyLiveCamera(panCamera(
        { scale: cam.scale, tx: panRef.current.tx, ty: panRef.current.ty },
        e.clientX - panRef.current.x,
        e.clientY - panRef.current.y,
      ));
      return;
    }
    const over = (e.target as HTMLElement).closest("[data-node-id]");
    const hid = over?.getAttribute("data-node-id") ?? null;
    if (boardHoverRef.current !== hid) {
      boardHoverRef.current = hid;
      paintFocus();
    }
    const drag = dragRef.current;
    if (! drag) {
      return;
    }
    const rect = pane.getBoundingClientRect();
    const world = worldFromScreen(cam, e.clientX - rect.left, e.clientY - rect.top);
    const live: ChipDrag = {
      id: drag.id,
      x: snapToGrid(world.x - drag.dx),
      y: snapToGrid(world.y - drag.dy),
    };
    chipDragRef.current = live;
    const el = pane.querySelector(`[data-node-id="${drag.id}"]`);
    if (el) {
      applyChipDragStyle(el as HTMLElement, live, { x: drag.originX, y: drag.originY }, drag.id);
    }
    paintCablesNow();
  }, [applyLiveCamera, paintFocus]);

  const onPointerUp = useCallback(() => {
    const drag = connectDragRef.current;
    if (drag?.snapPortId) {
      const dst = useBoardStore.getState().ports[drag.snapPortId];
      if (dst) {
        commitBoardConnect(drag.fromPort, dst);
      }
    }
    connectDragRef.current = null;
    paneRef.current?.querySelectorAll(".nk-port-hot").forEach((el) => el.classList.remove("nk-port-hot"));
    if (panRef.current) {
      camGesture.current = false;
      useBoardStore.getState().setCamera(camLive.current);
      paintCablesNow();
    }
    panRef.current = null;
    const chipDrag = dragRef.current;
    const live = chipDragRef.current;
    if (chipDrag && live) {
      useBoardStore.getState().moveNode(chipDrag.id, live.x, live.y);
      const el = paneRef.current?.querySelector(`[data-node-id="${chipDrag.id}"]`);
      if (el) {
        applyChipDragStyle(el as HTMLElement, null, { x: chipDrag.originX, y: chipDrag.originY }, chipDrag.id);
      }
      chipDragRef.current = null;
      rerouteBoard();
    }
    dragRef.current = null;
  }, []);

  useEffect(() => {
    const down = (e: KeyboardEvent) => {
      if (e.code === "Space") {
        spaceRef.current = true;
      }
      const tag = (e.target as HTMLElement | null)?.tagName;
      if (tag === "INPUT" || tag === "TEXTAREA" || tag === "SELECT") {
        return;
      }
      const view = { w: paneRef.current?.clientWidth || 960, h: paneRef.current?.clientHeight || 420 };
      if (isArrangeChord(e) && ! e.shiftKey) {
        e.preventDefault();
        void layoutBoard("ARRANGE", view);
      }
      if (e.shiftKey && ! e.ctrlKey && ! e.metaKey && e.key.toLowerCase() === "a") {
        e.preventDefault();
        void layoutBoard("COMPACT", view);
      }
      if (e.key === "Delete" || e.key === "Backspace") {
        const id = selectedRef.current;
        const n = id ? useBoardStore.getState().nodes[id] : undefined;
        if (id && canDeleteChip(n)) {
          e.preventDefault();
          removeCircuitBlock(id);
          setSelected(null);
        }
      }
    };
    const up = (e: KeyboardEvent) => {
      if (e.code === "Space") {
        spaceRef.current = false;
      }
    };
    window.addEventListener("keydown", down);
    window.addEventListener("keyup", up);
    return () => {
      window.removeEventListener("keydown", down);
      window.removeEventListener("keyup", up);
    };
  }, []);

  const onWheel = useCallback((e: WheelEvent<HTMLDivElement>) => {
    e.preventDefault();
    const pane = paneRef.current;
    if (! pane) {
      return;
    }
    const rect = pane.getBoundingClientRect();
    const sx = e.clientX - rect.left;
    const sy = e.clientY - rect.top;
    applyLiveCamera(zoomCamera(camLive.current, sx, sy, e.deltaY > 0 ? 0.92 : 1.08), true);
  }, [applyLiveCamera]);

  const graph = { nodes, ports, edges };
  const portsByNode = useMemo(() => {
    const m: Record<string, BoardPort[]> = {};
    for (const p of Object.values(ports)) {
      (m[p.nodeId] ??= []).push(p);
    }
    return m;
  }, [ports]);
  const bindAim = useMemo(() => {
    if (! bindLetter) {
      return null;
    }
    const pane = paneRef.current;
    if (! pane) {
      return null;
    }
    const box = pane.getBoundingClientRect();
    const world = worldFromScreen(camLive.current, bindX - box.left, bindY - box.top);
    const id = chipAtWorld(Object.values(nodes), world);
    const n = id ? nodes[id] : undefined;
    if (! n) {
      return null;
    }
    return { id, local: { x: world.x - n.x, y: world.y - n.y } };
  }, [bindLetter, bindX, bindY, nodes]);

  return (
    <div
      ref={paneRef}
      className="nk-board nk-circuit relative h-full min-h-0 w-full overflow-hidden bg-black"
      onPointerDown={onPointerDown}
      onPointerMove={onPointerMove}
      onPointerUp={onPointerUp}
      onPointerCancel={onPointerUp}
      onWheel={onWheel}
      onDoubleClick={(e) => {
        const id = (e.target as HTMLElement).closest("[data-node-id]")?.getAttribute("data-node-id");
        const n = id ? useBoardStore.getState().nodes[id] : undefined;
        if (n) {
          inspectChip(n.id, n.type);
        }
      }}
      onContextMenu={(e) => {
        e.preventDefault();
        const pane = paneRef.current;
        if (! pane) {
          return;
        }
        const box = pane.getBoundingClientRect();
        const world = worldFromScreen(camLive.current, e.clientX - box.left, e.clientY - box.top);
        const chipId = (e.target as HTMLElement).closest("[data-node-id]")?.getAttribute("data-node-id") ?? null;
        const hit = boardContextHit(chipId, world, useBoardStore.getState());
        const at = { left: e.clientX - box.left, top: e.clientY - box.top };
        if (hit.kind === "chip") {
          setSelected(hit.id);
          setMenu({ kind: "chip", ...at, id: hit.id });
          return;
        }
        if (hit.kind === "edge") {
          setMenu({ kind: "add", ...at, afterId: hit.sourceId });
          return;
        }
        setMenu({ kind: "pane", ...at, world });
      }}
      onClick={(e) => {
        if (shouldCollapseChipDetail(e.target, useBindStore.getState().letter)) {
          if (!(e.target as HTMLElement).closest("[data-node-id]")) {
            useChipViewStore.getState().collapseAll();
            setSelected(null);
          }
        }
      }}
    >
      <CableCanvas
        graph={graph}
        cameraRef={camLive}
        gestureRef={camGesture}
        width={size.w}
        height={size.h}
        active={active}
      />
      <div
        ref={worldRef}
        className="nk-board-world"
        style={{ transform: cameraMatrix(camLive.current) }}
      >
        {Object.values(nodes).map((n) => (
          <BoardChip
            key={n.id}
            node={n}
            ports={portsByNode[n.id] ?? []}
            selected={selected === n.id}
            bindOver={bindAim?.id === n.id}
            bindLocal={bindAim?.id === n.id ? bindAim.local : { x: 0, y: 0 }}
            onInspect={() => inspectChip(n.id, n.type)}
          />
        ))}
      </div>
      {menu?.kind === "pane" ? (
        <OsContextMenu left={menu.left} top={menu.top} title="Add" onDismiss={() => setMenu(null)}>
          <OsAddPicker onPick={(type, args) => {
            placeRef.current = menu.world;
            addCircuitBlock(type, args);
            setMenu(null);
          }} />
          <OsMenuItem onClick={() => { setMenu(null); void layoutBoard("ARRANGE", size); }}>Arrange</OsMenuItem>
          <OsMenuItem onClick={() => { setMenu(null); void layoutBoard("COMPACT", size); }}>Compact</OsMenuItem>
        </OsContextMenu>
      ) : null}
      {menu?.kind === "chip" ? (
        <OsContextMenu left={menu.left} top={menu.top} title={menu.id} onDismiss={() => setMenu(null)}>
          <OsMenuItem onClick={() => {
            setMenu({ kind: "add", left: menu.left, top: menu.top, afterId: menu.id });
          }}>Insert after</OsMenuItem>
          <OsMenuItem onClick={() => {
            const n = useBoardStore.getState().nodes[menu.id];
            if (n) {
              inspectChip(n.id, n.type);
            }
            setMenu(null);
          }}>Inspect</OsMenuItem>
          {canDeleteChip(nodes[menu.id]) ? (
            <OsMenuItem danger onClick={() => {
              removeCircuitBlock(menu.id);
              setSelected(null);
              setMenu(null);
            }}>Delete</OsMenuItem>
          ) : null}
        </OsContextMenu>
      ) : null}
      {menu?.kind === "add" ? (
        <OsContextMenu left={menu.left} top={menu.top} title="Insert" onDismiss={() => setMenu(null)}>
          <OsAddPicker onPick={(type, args) => {
            placeRef.current = { afterId: menu.afterId };
            insertCircuitBlockAfter(menu.afterId, type, args);
            setMenu(null);
          }} />
        </OsContextMenu>
      ) : null}
    </div>
  );
}
