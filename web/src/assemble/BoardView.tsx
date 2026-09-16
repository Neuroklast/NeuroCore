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
import { addCircuitBlock, copyCircuitBlock, cutCircuitBlock, duplicateCircuitBlock, insertCircuitBlockAfter, parkCircuitBlock, pasteCircuitBlockAfter, removeCircuitBlock } from "./addBlock";
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
import { circuitPaintActive } from "../app/workspace";

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
  const layoutBusy = useBoardStore((s) => s.layoutBusy);
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
  const paintFocus = useCallback(() => {
    const pane = paneRef.current;
    const g = useBoardStore.getState();
    const allowed = circuitDofAllowed(motion, prefersReduced, camLive.current.scale);
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
    boardFocusEdgesRef.current = allowed && plane.active ? plane.edges : null;
    if (pane) {
      applyBoardFocus(pane.querySelectorAll("[data-node-id]"), (id) => focusAttr(allowed, plane, id));
    }
    paintCablesNow();
  }, [motion, prefersReduced]);

  useEffect(() => {
    const prevIds = Object.keys(useBoardStore.getState().nodes);
    const keep = keepBoardXy({
      origin: useAstStore.getState().origin,
      prevIds,
      nextIds: previewBoardIds(ast, sidechainOn),
      chipsHavePositions: true,
    });
    useBoardStore.getState().hydrate(ast, sidechainOn, keep);
  }, [ast, sidechainOn]);

  useEffect(() => {
    if (origin === "preset") {
      useChipViewStore.getState().collapseAll();
    }
  }, [origin, ast]);

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
    if (useBoardStore.getState().commandedLayout) {
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
        void layoutBoard("ARRANGE", view, { force: true });
      }
      if (e.shiftKey && ! e.ctrlKey && ! e.metaKey && e.key.toLowerCase() === "a") {
        e.preventDefault();
        void layoutBoard("COMPACT", view, { force: true });
      }
      const chord = e.ctrlKey || e.metaKey;
      const key = e.key.toLowerCase();
      const id = selectedRef.current;
      const n = id ? useBoardStore.getState().nodes[id] : undefined;
      if (chord && id && key === "c" && canDeleteChip(n)) { e.preventDefault(); copyCircuitBlock(id); return; }
      if (chord && id && key === "x" && canDeleteChip(n)) { e.preventDefault(); cutCircuitBlock(id); setSelected("IN"); return; }
      if (chord && key === "v") { e.preventDefault(); const fresh = pasteCircuitBlockAfter(id || "IN"); if (fresh) setSelected(fresh); return; }
      if (chord && id && key === "d" && canDeleteChip(n)) { e.preventDefault(); const fresh = duplicateCircuitBlock(id); if (fresh) setSelected(fresh); return; }
      if (chord && id && key === "p" && canDeleteChip(n)) { e.preventDefault(); parkCircuitBlock(id); return; }
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
    paintFocus();
  }, [applyLiveCamera, paintFocus]);

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
        active={active && ! layoutBusy && circuitPaintActive("assemble", size.w, size.h)}
      />
      {layoutBusy ? (
        <div
          className="absolute inset-0 z-40 flex flex-col items-center justify-center bg-black/85"
          role="progressbar"
          aria-busy="true"
          aria-label="Layout"
        >
          <div className="text-[11px] tracking-[0.35em] text-ink">LAYOUT</div>
          <div className="mt-3 h-1 w-48 overflow-hidden bg-ink-muted">
            <div className="nk-layout-bar h-full w-full bg-accent" />
          </div>
        </div>
      ) : null}
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
          <OsMenuItem onClick={() => { setMenu(null); void layoutBoard("ARRANGE", size, { force: true }); }}>Arrange</OsMenuItem>
          <OsMenuItem onClick={() => { setMenu(null); void layoutBoard("COMPACT", size, { force: true }); }}>Compact</OsMenuItem>
        </OsContextMenu>
      ) : null}
      {menu?.kind === "chip" ? (
        <OsContextMenu left={menu.left} top={menu.top} title={menu.id} onDismiss={() => setMenu(null)}>
          <OsMenuItem onClick={() => {
            setMenu({ kind: "add", left: menu.left, top: menu.top, afterId: menu.id });
          }}>Insert after</OsMenuItem>
          {canDeleteChip(nodes[menu.id]) ? <>
            <OsMenuItem onClick={() => { copyCircuitBlock(menu.id); setMenu(null); }}>Copy</OsMenuItem>
            <OsMenuItem onClick={() => { cutCircuitBlock(menu.id); setSelected("IN"); setMenu(null); }}>Cut</OsMenuItem>
            <OsMenuItem onClick={() => { const id = duplicateCircuitBlock(menu.id); if (id) setSelected(id); setMenu(null); }}>Duplicate</OsMenuItem>
            <OsMenuIte…341 tokens truncated…m "vitest";
import {
  ADDABLE_BLOCKS,
  ADD_CATEGORIES,
  blocksInCategory,
  nextBlockId,
  scriptAfterAdd,
  scriptAfterInsertAfter,
  scriptAfterRemove,
  scriptAfterRename,
  scriptAfterSetArg,
} from "./addBlock";
import { chipSpec } from "./chipSpec";
import { useAstStore } from "../store/astStore";

describe("circuit add/remove", () => {
  it("lists addable chips and inserts a legal line before out", () => {
    expect(ADDABLE_BLOCKS.some((b) => b.type === "filter")).toBe(true);
    const next = scriptAfterAdd("stage1: y = x\nout: main = 1\n", "filter");
    expect(next).toContain("filter1: type = lowpass");
    expect(next).toContain("bus __park:");
    expect(next.indexOf("bus __park:")).toBeLessThan(next.indexOf("filter1:"));
    expect(next.indexOf("stage1:")).toBeLessThan(next.indexOf("bus __park:"));
    expect(next.indexOf("filter1")).toBeLessThan(next.indexOf("out:"));
    expect(nextBlockId("stage", ["stage1"])).toBe("stage2");
  });

  it("inserts after a chip in the serial chain, not on bus __park", () => {
    const next = scriptAfterInsertAfter("stage1: y = x\nout: main = 1\n", "stage1", "filter");
    expect(next).toMatch(/stage1:[\s\S]*filter1:/);
    expect(next.indexOf("stage1:")).toBeLessThan(next.indexOf("filter1:"));
    expect(next.indexOf("filter1:")).toBeLessThan(next.indexOf("out:"));
    expect(next).not.toContain("bus __park:");
  });

  it("inserts after IN at the head of the chain, not on the park bus", () => {
    const next = scriptAfterInsertAfter("stage1: y = x\nout: main = 1\n", "IN", "filter");
    expect(next).not.toContain("bus __park:");
    expect(next.indexOf("filter1:")).toBeLessThan(next.indexOf("stage1:"));
    expect(next.indexOf("filter1:")).toBeLessThan(next.indexOf("out:"));
  });

  it("groups Add items by category and inserts a custom formula block", () => {
    expect([...ADD_CATEGORIES]).toEqual(expect.arrayContaining(["Dynamics", "Tone", "Custom"]));
    expect(blocksInCategory("Custom").map((b) => b.type)).toEqual(["custom"]);
    const next = scriptAfterAdd("filter1: type = lowpass; cutoff = 800\n", "custom");
    expect(next).toMatch(/custom1:\s*y = x/);
    const edited = scriptAfterSetArg(next, "custom1", "in2", "0");
    expect(edited).toContain("in2 = 0");
    expect(edited).toContain("y = x");
  });

  it("drops a named block from the script", () => {
    const gone = scriptAfterRemove("stage1: y = x\nfilter1: type = lowpass; cutoff = 800\n", "filter1");
    expect(gone).not.toMatch(/filter1/);
    expect(gone).toContain("stage1");
  });

  it("offers Split/Join Mid-Side and L/R and emits canonical msN mode", () => {
    const routing = ADDABLE_BLOCKS.filter((b) => b.category === "Routing");
    const labels = routing.map((b) => b.label);
    expect(labels).toEqual(expect.arrayContaining([
      "Split Mid/Side",
      "Join Mid/Side",
      "Split L/R",
      "Join L/R",
    ]));
    expect(labels.some((l) => l === "MS Enc" || l === "MS Dec")).toBe(false);

    const byLabel = (label: string) => routing.find((b) => b.label === label);
    expect(byLabel("Split Mid/Side")?.type).toBe("ms");
    expect(byLabel("Split Mid/Side")?.args).toMatch(/mode\s*=\s*split\b/);
    expect(byLabel("Join Mid/Side")?.args).toMatch(/mode\s*=\s*join\b/);
    expect(byLabel("Split L/R")?.args).toMatch(/mode\s*=\s*split\b/);
    expect(byLabel("Split L/R")?.args).toMatch(/family\s*=\s*lr\b/);
    expect(byLabel("Join L/R")?.args).toMatch(/mode\s*=\s*join\b/);
    expect(byLabel("Join L/R")?.args).toMatch(/family\s*=\s*lr\b/);

    const split = scriptAfterAdd("stage1: y = x\n", "ms", "mode = split");
    expect(split).toMatch(/ms1:\s*mode = split/);
    const joinLr = scriptAfterAdd(split, "ms", "mode = join; family = lr");
    expect(joinLr).toMatch(/ms2:\s*mode = join;\s*family = lr/);
  });

  it("offers Bus and Join Signal and emits bus dirt: plus mix", () => {
    const routing = ADDABLE_BLOCKS.filter((b) => b.category === "Routing");
    const byLabel = (label: string) => routing.find((b) => b.label === label);
    expect(byLabel("Bus")?.type).toBe("bus");
    expect(byLabel("Bus")?.args).toMatch(/name\s*=\s*dirt\b/);
    expect(byLabel("Join Signal")?.type).toBe("join");
    expect(byLabel("Join Signal")?.args).toMatch(/mix\s*=\s*0\.5\b/);

    const withBus = scriptAfterAdd("stage1: y = x\n", "bus", "name = dirt");
    expect(withBus).toMatch(/^bus dirt:\s*$/m);
    expect(withBus).not.toMatch(/\bbus1\s*:/);
    const withDelay = scriptAfterAdd(withBus, "delay");
    expect(withDelay.indexOf("bus dirt:")).toBeLessThan(withDelay.indexOf("delay1:"));
    const withJoin = scriptAfterAdd(withDelay, "join", "mix = 0.5");
    expect(withJoin).toMatch(/join1:\s*mix = 0\.5/);
    expect(withJoin).not.toMatch(/^\s*out\s*:/m);
  });

  it("offers Multiband Split, Send, Width, Octaver with catalog defaults", () => {
    const byLabel = (label: string) => ADDABLE_BLOCKS.find((b) => b.label === label);
    expect(byLabel("Xover")).toBeUndefined();
    expect(byLabel("Multiband Split")?.type).toBe("xover");
    expect(byLabel("Multiband Split")?.args).toMatch(/f1\s*=\s*200/);
    expect(byLabel("Multiband Split")?.args).toMatch(/f2\s*=\s*2000/);

    expect(byLabel("Send")?.type).toBe("send");
    expect(byLabel("Send")?.args).toMatch(/kanal\s*=\s*both/);
    expect(chipSpec("send").enums.kanal).toEqual(["both", "left", "right", "mid", "side", "env"]);

    expect(byLabel("Width")?.type).toBe("widen");
    expect(byLabel("Width")?.args).toMatch(/width\s*=/);
    expect(byLabel("Width")?.args).toMatch(/delay\s*=/);
    expect(byLabel("Width")?.args).toMatch(/bass\s*=/);
    expect(chipSpec("width").paramJacks).toEqual(["width", "delay", "bass"]);

    expect(byLabel("Octaver")?.type).toBe("octaver");
    expect(chipSpec("octaver").paramJacks).toEqual(["sub", "up", "mix", "tone", "thresh"]);

    expect(byLabel("Phaser")?.type).toBe("phaser");
    expect(chipSpec("phaser").paramJacks).toEqual(["stages", "rate", "depth", "center", "feedback", "mix"]);
    expect(byLabel("Flanger")?.type).toBe("flanger");
    expect(chipSpec("flanger").enums.invert).toEqual(["off", "on"]);
  });

  it("offers Cabinet IR and emits ir1: mix / gain", () => {
    const cab = ADDABLE_BLOCKS.find((b) => b.label === "Cabinet IR");
    expect(cab?.type).toBe("ir");
    expect(cab?.category).toBe("Time");
    expect(cab?.args).toMatch(/mix\s*=\s*0\.3/);
    expect(cab?.args).toMatch(/gain\s*=\s*0/);
    const next = scriptAfterAdd("stage1: y = x\n", "ir");
    expect(next).toMatch(/ir1:\s*mix = 0\.3/);
    expect(next).toContain("gain = 0");
    expect(nextBlockId("ir", ["ir1"])).toBe("ir2");
  });

  it("rewrites a custom chip id across the script on rename", () => {
    const src = "custom1: y = x * a\nfilter1: type = lowpass; cutoff = custom1\nout: main = 1\n";
    const next = scriptAfterRename(src, "custom1", "dirt");
    expect(next).toMatch(/^dirt:\s*y = x \* a/m);
    expect(next).toContain("cutoff = dirt");
    expect(next).not.toMatch(/\bcustom1\b/);
    expect(scriptAfterRename(src, "custom1", "custom1")).toBe(src);
    expect(scriptAfterRename(src, "custom1", "")).toBe(src);
  });
});

describe('Circuit clipboard commands',()=>{
  it('copies, duplicates, cuts, pastes and parks a selected chip through the script', async()=>{
    const { applyCanvasScript, copyCircuitBlock, cutCircuitBlock, duplicateCircuitBlock, pasteCircuitBlockAfter, parkCircuitBlock } = await import('./addBlock');
    applyCanvasScript('stage1: y = tanh(x*3)\nfilter1: type = highpass; cutoff = 900\nout: main = 1\n');
    expect(copyCircuitBlock('stage1')).toBe(true);
    expect(duplicateCircuitBlock('stage1')).toMatch(/^stage2$/);
    expect(useAstStore.getState().lastValidScript).toContain('stage2: y = tanh(x*3)');
    expect(cutCircuitBlock('stage2')).toBe(true);
    expect(useAstStore.getState().lastValidScript).not.toContain('stage2:');
    expect(pasteCircuitBlockAfter('filter1')).toMatch(/^stage2$/);
    parkCircuitBlock('filter1');
    expect(useAstStore.getState().lastValidScript).toMatch(/bus __park:\nfilter1:/);
  });
});
