import { PortBrowser } from "./PortBrowser";
import { useCallback, useEffect, useLayoutEffect, useMemo, useRef, useState, type PointerEvent, type WheelEvent } from "react";
import { hasJuceBridge } from "../bridge/juce";
import { useAstStore } from "../store/astStore";
import { parseClipPeaks, useHostStore } from "../store/hostStore";
import { subscribeVizClock } from "../theme/vizClock";
import { shouldCollapseChipDetail, useChipViewStore } from "../store/expandStore";
import { useBindStore } from "../store/telemetryStore";
import { OsAddPicker, OsContextMenu, OsMenuItem } from "../overlays/OsContextMenu";
import { undoTargetIsText } from "../chrome/undoModel";
import { isArrangeChord } from "../chrome/shortcuts";
import { chipOverlay } from "../presets/irSlots";
import { addCircuitBlock, copyCircuitBlock, cutCircuitBlock, duplicateCircuitBlock, insertCircuitBlockAfter, insertCircuitBlockOnEdge, parkCircuitBlock, pasteCircuitBlockAfter, removeCircuitBlock } from "./addBlock";
import { BoardNavigator } from "./BoardNavigator";
import { BoardChip } from "./BoardChip";
import { BoardTerminal } from "./BoardTerminal";
import { ADD_PICKER_BOX, CHIP_MENU_BOX, clientToPane, placeOverlay, placeOverlayPane } from "../theme/fit";
import { wheelOwnsZoom } from "./boardWheel";
import { applyCameraTransform, cameraMatrix, fitCamera, fitBoardCamera, panCamera, shouldFitCameraOnLateLayout, shouldFitCameraOnPaneSize, screenFromWorld, worldFromScreen, zoomCamera } from "./boardCamera";
import { applyBoardZoom } from "./nodeFace/semanticZoom";
import { commitBoardConnect, commitBoardCut, layoutBoard, rerouteBoard } from "./boardCommit";
import { connectDragRef, magnetPort } from "./boardConnect";
import { boardContextHit, canDeleteChip, chipAtWorld, circuitAllowsTextSelect, edgeRoute } from "./boardEdit";
import { graphToLayout, portGlobal, portIsOutput, previewBoardIds, type BoardPort } from "./boardModel";
import { applyChipDragStyle, chipDragGroupRef, chipDragRef, type ChipDrag } from "./boardDrag";
import { useBoardStore } from "./boardStore";
import { keepBoardXy } from "./boardSync";
import { CableCanvas, paintCablesNow } from "./CableCanvas";
import { demoClipRows } from "./demoClips";
import { applyBoardFocus, applyDofGate, boardFocusEdgesRef, boardHoverRef, circuitDofAllowed, focusAttr, focusPlane } from "./circuitDof";
import { CHIP_AIR_X, CHIP_AIR_Y, snapToGrid } from "./grid";
import { serialIds, wrapFits } from "./layout/compactPack";
import { circuitPaintActive } from "../app/workspace";
import { shortestPath } from "./boardPath";
import { nodesInSelection, routeMidpoint, type SelectionRect } from "./boardSelection";
import { boardIdForSource } from "../hack/dslSourceMap";
import { pruneSelection, publishSelection } from "../store/patchWorkspace";
import { usePatchSelectionStore } from "../store/patchSelectionStore";
import { setGroupMute, setGroupSolo } from "./muteSoloApply";
import { isFlowBlockId } from "./muteSolo";

type BoardMenu =
  | { kind: "pane"; left: number; top: number; world: { x: number; y: number } }
  | { kind: "chip"; left: number; top: number; id: string }
  | { kind: "add"; left: number; top: number; afterId: string; targetId?: string };

function capturePanePointer(pane: HTMLElement, pointerId: number) {
  try {
    pane.setPointerCapture(pointerId);
  } catch {
    /* synthetic pointerdown has no active pointer id */
  }
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
  const paneSizeRef = useRef({ w: 0, h: 0 });
const panRef = useRef<{ startX: number; startY: number; tx: number; ty: number } | null>(null);
const dragRef = useRef<{ id: string; dx: number; dy: number; originX: number; originY: number; startX: number; startY: number; group: Record<string, { x: number; y: number }> } | null>(null);
const lassoRef = useRef<SelectionRect | null>(null);
  const lassoCompletedRef = useRef(false);
  const [lasso, setLasso] = useState<SelectionRect | null>(null);
  const spaceRef = useRef(false);
  const rawSelection = usePatchSelectionStore((s) => s.nodeIds);
  const boardIds = Object.keys(nodes);
  const selection = rawSelection
    .map((id) => boardIdForSource(id, boardIds) ?? id)
    .filter((id) => Boolean(nodes[id]));
  const selected = selection.length ? selection[selection.length - 1]! : null;
  const selectedRef = useRef<string | null>(null);
  selectedRef.current = selected;
  const selectionRef = useRef<string[]>([]);
  selectionRef.current = selection;
  const selectNodes = (ids: string[] | ((old: string[]) => string[])) => {
    const old = usePatchSelectionStore.getState().nodeIds;
    const next = typeof ids === "function" ? ids(old) : ids;
    publishSelection({
      nodeIds: next,
      primaryId: next.length ? next[next.length - 1]! : null,
      param: null,
      origin: "circuit",
      intent: "mark",
    });
  };
  const inspectChip = (id: string, type: string) => {
    publishSelection({
      nodeIds: [id],
      primaryId: id,
      param: null,
      origin: "inspect",
      intent: "mark",
    });
    const o = chipOverlay(id, type);
    useHostStore.getState().setOverlay(o.overlay, o.inspectId);
  };
  const clearBoardFocus = () => {
    selectNodes([]);
  };
  const [portBrowserId,setPortBrowserId]=useState<string|null>(null);
  const [menu, setMenu] = useState<BoardMenu | null>(null);
  const overlayOpenRef = useRef(false);
  overlayOpenRef.current = menu != null;
  const [insertError, setInsertError] = useState("");
  useEffect(() => setInsertError(""), [menu]);
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
    const allowed = circuitDofAllowed(motion, prefersReduced);
    const plane = focusPlane({
      selectedNodeIds: selectionRef.current,
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
      applyDofGate(pane, allowed);
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
    let lastDemo = 0;
    return subscribeVizClock((now) => {
      if (now - lastDemo < 125) {
        return;
      }
      lastDemo = now;
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
        clipsPeaks: parsed.peaks,
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
        rerouteBoard();
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
      const next = { w: el.clientWidth, h: el.clientHeight };
      if (next.w < 40 || next.h < 40) return;
      const prev = paneSizeRef.current;
      paneSizeRef.current = next;
      setSize(next);
      const moved = useBoardStore.getState().userMoved;
      if (!useBoardStore.getState().layoutBusy && shouldFitCameraOnPaneSize(prev.w, prev.h, next.w, next.h, moved)) {
        const ns = Object.values(useBoardStore.getState().nodes);
        if (ns.length > 0) {
          useBoardStore.getState().setCamera(fitBoardCamera(useBoardStore.getState(), next));
        }
      }
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

  const nodeIdKey = Object.keys(nodes).sort().join("\0");
  useEffect(() => {
    if (size.w < 40 || useBoardStore.getState().layoutBusy) {
      return;
    }
    if (! shouldFitCameraOnLateLayout({
      commanded: false,
      userMoved,
      overlayOpen: overlayOpenRef.current,
      cameraChanged: false,
    })) {
      return;
    }
    const ns = Object.values(useBoardStore.getState().nodes);
    if (ns.length === 0) {
      return;
    }
    useBoardStore.getState().setCamera(fitBoardCamera(useBoardStore.getState(), size));
  }, [nodeIdKey, userMoved]);

  const applyLiveCamera = useCallback((cam: typeof camLive.current, commit = false) => {
    camLive.current = cam;
    (window as Window & { __nkBoardCamera?: typeof cam }).__nkBoardCamera = cam;
    if (worldRef.current) {
      applyCameraTransform(worldRef.current, cam);
    }
    applyBoardZoom(paneRef.current, cam.scale);
    paintCablesNow();
    if (commit) {
      useBoardStore.getState().setCamera(cam);
    }
  }, []);

  useEffect(() => {
    const w = window as Window & { __nkBoardCamera?: typeof camLive.current };
    w.__nkBoardCamera = camLive.current;
    return () => {
      delete w.__nkBoardCamera;
    };
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
  }, [selection, nodes, paintFocus]);

  useEffect(() => {
    pruneSelection(Object.keys(nodes));
  }, [nodes]);

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
    const view = { w: paneRef.current?.clientWidth || size.w || 960, h: paneRef.current?.clientHeight || size.h || 420 };
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
  }, [ast, origin, userMoved, Object.keys(nodes).length]);

  const handleContextMenu = (e: PointerEvent<HTMLDivElement>, pane: HTMLElement, world: { x: number; y: number }) => {
    const chipId = (e.target as HTMLElement).closest("[data-node-id]")?.getAttribute("data-node-id") ?? null;
    const hit = boardContextHit(chipId, world, useBoardStore.getState());
    if (hit.kind === "chip") {
      // Bereits selektierte Chips behalten die Mehrfachauswahl; nur primary setzen.
      const cur = usePatchSelectionStore.getState();
      if (cur.nodeIds.includes(hit.id)) {
        publishSelection({ nodeIds: cur.nodeIds, primaryId: hit.id, origin: "circuit", intent: "mark" });
      } else {
        selectNodes([hit.id]);
      }
      setMenu({ kind: "chip", ...placeOverlay(pane, e.clientX, e.clientY, CHIP_MENU_BOX), id: hit.id });
      return;
    }
    if (hit.kind === "edge") {
      setMenu({ kind: "add", ...placeOverlay(pane, e.clientX, e.clientY, ADD_PICKER_BOX), afterId: hit.sourceId, targetId: hit.targetId });
      return;
    }
    setMenu({ kind: "pane", ...placeOverlay(pane, e.clientX, e.clientY, ADD_PICKER_BOX), world });
  };

  const onPointerDown = useCallback((e: PointerEvent<HTMLDivElement>) => {
    const pane = paneRef.current;
    if (! pane) {
      return;
    }
    const target = e.target as HTMLElement;

    // 1. Aktive native Controls und Eingabefelder starten nie eine Gesture.
    if (target.closest("[data-board-control], input, textarea, select, [contenteditable]")) {
      return;
    }

    const cam = camLive.current;
    const panePt = clientToPane(pane, e.clientX, e.clientY);
    const world = worldFromScreen(cam, panePt.x, panePt.y);

    // Rechtsklick → Kontextmenü, niemals eine Kamera- oder Chip-Verschiebung.
    if (e.button === 2) {
      e.preventDefault();
            handleContextMenu(e, pane, world);
      return;
    }
    if (e.button !== 0) {
      return;
    }

    // 2. Port- und Binding-Verbindungen.
    const portEl = target.closest("[data-port-id]");
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
        const from = portGlobal(node, port);
        connectDragRef.current = {
          fromPort: port,
          from,
          to: from,
          snapPortId: null,
          kind: port.kind,
        };
        capturePanePointer(pane, e.pointerId);
                return;
      }
    }

    // 3. Chip: Selektion und (nach Schwelle) gemeinsamer Chip-Drag.
    // Ein Klick auf eine Chip-Kontrolle (M/S, Chevron, Bind-Jack, Overlay) startet
    // weder eine Kamera- noch eine Chip-Geste; der Control behält sein eigenes onClick.
    const chip = target.closest("[data-node-id]");
    const id = chip?.getAttribute("data-node-id");
    if (id) {
      if (! target.closest(".nk-port, .nk-chip-expand, .nk-ms, .nk-chip-overlay, .nk-bind-jack, .nk-bind-cell")) {
        const n = useBoardStore.getState().nodes[id];
        if (e.ctrlKey || e.metaKey) {
          selectNodes((old) => old.includes(id) ? old.filter((v) => v !== id) : [...old, id]);
                    return;
        }
        if (e.shiftKey) {
          const anchor = selectedRef.current;
          if (anchor && anchor !== id) {
            const g = useBoardStore.getState();
            const path = shortestPath({ nodes: g.nodes, edges: g.edges }, anchor, id);
            if (path.length > 0) {
              selectNodes(path);
            } else {
              selectNodes([id]);
            }
          } else {
            selectNodes([id]);
          }
                    return;
        }
        const group = selectionRef.current.includes(id) ? selectionRef.current : [id];
        selectNodes(group);
        if (! n || n.locked) {
                    return;
        }
        const origins = Object.fromEntries(group.filter((v) => !useBoardStore.getState().nodes[v]?.locked).map((v) => {
          const node = useBoardStore.getState().nodes[v]!;
          return [v, { x: node.x, y: node.y }];
        }));
        dragRef.current = { id, dx: world.x - n.x, dy: world.y - n.y, originX: n.x, originY: n.y,
          startX: e.clientX, startY: e.clientY, group: origins };
        capturePanePointer(pane, e.pointerId);
                return;
      }
      // Control auf einem Chip: keine Kamera-Geste, keine Selektions-Aenderung.
            return;
    }

    // 4. Freier Hintergrund: Alt + Drag = Rechteckauswahl, sonst Kamera-Pan.
    if (e.altKey) {
      lassoRef.current = { x0: world.x, y0: world.y, x1: world.x, y1: world.y };
      setLasso(lassoRef.current);
            capturePanePointer(pane, e.pointerId);
      return;
    }
    panRef.current = { startX: e.clientX, startY: e.clientY, tx: cam.tx, ty: cam.ty };
    camGesture.current = true;
    capturePanePointer(pane, e.pointerId);
      }, []);

  const onPointerMove = useCallback((e: PointerEvent<HTMLDivElement>) => {
    const pane = paneRef.current;
    if (! pane) {
      return;
    }
    const cam = camLive.current;
    const target = e.target as HTMLElement;

    if (connectDragRef.current) {
      const panePt = clientToPane(pane, e.clientX, e.clientY);
      const world = worldFromScreen(cam, panePt.x, panePt.y);
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
      const dx = e.clientX - panRef.current.startX;
      const dy = e.clientY - panRef.current.startY;
      applyLiveCamera(panCamera(
        { scale: cam.scale, tx: panRef.current.tx, ty: panRef.current.ty },
        dx,
        dy,
      ));
      return;
    }
    if (lassoRef.current) {
      const panePt = clientToPane(pane, e.clientX, e.clientY);
      const world = worldFromScreen(cam, panePt.x, panePt.y);
      lassoRef.current = { ...lassoRef.current, x1: world.x, y1: world.y };
      setLasso(lassoRef.current);
      return;
    }
    const over = target.closest("[data-node-id]");
    const hid = over?.getAttribute("data-node-id") ?? null;
    if (boardHoverRef.current !== hid) {
      boardHoverRef.current = hid;
      paintFocus();
    }
    const drag = dragRef.current;
    if (! drag) {
      return;
    }
    if (Math.hypot(e.clientX - drag.startX, e.clientY - drag.startY) < 4) return;
    const panePt = clientToPane(pane, e.clientX, e.clientY);
    const world = worldFromScreen(cam, panePt.x, panePt.y);
    const live: ChipDrag = {
      id: drag.id,
      x: world.x - drag.dx,
      y: world.y - drag.dy,
    };
    chipDragRef.current = live;
    chipDragGroupRef.current = Object.fromEntries(Object.entries(drag.group).map(([id, origin]) => [id, {
      id, x: origin.x + live.x - drag.originX, y: origin.y + live.y - drag.originY,
    }]));
    const el = pane.querySelector(`[data-node-id="${drag.id}"]`);
    if (el) {
      applyChipDragStyle(el as HTMLElement, live, { x: drag.originX, y: drag.originY }, drag.id);
    }
    for (const id of Object.keys(drag.group)) {
      if (id === drag.id) continue;
      const other = pane.querySelector(`[data-node-id="${id}"]`) as HTMLElement | null;
      if (other) other.style.transform = `translate(${live.x - drag.originX}px, ${live.y - drag.originY}px)`;
    }
    paintCablesNow();
  }, [applyLiveCamera, paintFocus]);

  const onPointerUp = useCallback((e: PointerEvent<HTMLDivElement>) => {
    const drag = connectDragRef.current;
    if (drag?.snapPortId) {
      const dst = useBoardStore.getState().ports[drag.snapPortId];
      if (dst) {
        void (portIsOutput(drag.fromPort) ? commitBoardConnect(drag.fromPort, dst) : commitBoardConnect(dst, drag.fromPort));
      }
    }
    connectDragRef.current = null;
    paneRef.current?.querySelectorAll(".nk-port-hot").forEach((el) => el.classList.remove("nk-port-hot"));
    
    if (panRef.current) {
      camGesture.current = false;
      useBoardStore.getState().setCamera(camLive.current);
      paintCablesNow();
      // Ein reiner Klick (ohne nennenswerte Bewegung) auf dem Hintergrund deselektiert.
      const moved = Math.hypot(e.clientX - panRef.current.startX, e.clientY - panRef.current.startY);
      panRef.current = null;
      if (moved < 4) {
        clearBoardFocus();
      }
      return;
    }
    if (lassoRef.current) {
      const ids = nodesInSelection(Object.values(useBoardStore.getState().nodes), lassoRef.current);
      selectNodes((old) => [...new Set([...old, ...ids])]);
      lassoCompletedRef.current = true;
      lassoRef.current = null;
      setLasso(null);
      return;
    }
    const chipDrag = dragRef.current;
    const live = chipDragRef.current;
    if (chipDrag && live) {
      const dx = snapToGrid(live.x) - chipDrag.originX;
      const dy = snapToGrid(live.y) - chipDrag.originY;
      for (const [id, origin] of Object.entries(chipDrag.group)) {
        useBoardStore.getState().moveNode(id, origin.x + dx, origin.y + dy);
      }
      const el = paneRef.current?.querySelector(`[data-node-id="${chipDrag.id}"]`);
      if (el) {
        applyChipDragStyle(el as HTMLElement, null, { x: chipDrag.originX, y: chipDrag.originY }, chipDrag.id);
      }
      chipDragRef.current = null;
      chipDragGroupRef.current = null;
      for (const id of Object.keys(chipDrag.group)) {
        const other = paneRef.current?.querySelector(`[data-node-id="${id}"]`) as HTMLElement | null;
        if (other) other.style.transform = "";
      }
      rerouteBoard();
    }
    dragRef.current = null;
  }, []);

  useEffect(() => {
    const down = (e: KeyboardEvent) => {
      if (!active || undoTargetIsText(e.target) || useHostStore.getState().overlay != null) return;
      if (e.code === "Space") {
        spaceRef.current = true;
      }
      const tag = (e.target as HTMLElement | null)?.tagName;
      if (tag === "INPUT" || tag === "TEXTAREA" || tag === "SELECT") {
        return;
      }
      const view = { w: paneRef.current?.clientWidth || 960, h: paneRef.current?.clientHeight || 420 };
      if (isArrangeChord(e) && e.shiftKey) {
        e.preventDefault();
        void layoutBoard("ARRANGE", view, { force: true });
        return;
      }
      if (e.shiftKey && ! e.ctrlKey && ! e.metaKey && e.key.toLowerCase() === "a") {
        e.preventDefault();
        void layoutBoard("COMPACT", view, { force: true });
      }
      const chord = e.ctrlKey || e.metaKey;
      const key = e.key.toLowerCase();
      if (chord && key === "a") { e.preventDefault(); selectNodes(Object.keys(useBoardStore.getState().nodes)); return; }
      if (key === "escape") { setMenu(null); clearBoardFocus(); return; }
      if (e.key === "ContextMenu" || (e.shiftKey && e.key === "F10")) {
        e.preventDefault();
        const pane = paneRef.current;
        if (! pane) {
          return;
        }
        const id = selectedRef.current;
        const n = id ? useBoardStore.getState().nodes[id] : undefined;
        if (n) {
          const s = screenFromWorld(camLive.current, n.x, n.y);
          const at = placeOverlayPane(pane, s.x + 8, s.y + 8, CHIP_MENU_BOX);
          selectNodes([n.id]);
          setMenu({ kind: "chip", ...at, id: n.id });
        } else {
          const at = placeOverlayPane(pane, 8, 8, ADD_PICKER_BOX);
          setMenu({ kind: "pane", ...at, world: worldFromScreen(camLive.current, at.left, at.top) });
        }
        return;
      }
      const id = selectedRef.current;
      const n = id ? useBoardStore.getState().nodes[id] : undefined;
      if (chord && id && key === "c" && canDeleteChip(n)) { e.preventDefault(); copyCircuitBlock(id); return; }
      if (chord && id && key === "x" && canDeleteChip(n)) { e.preventDefault(); cutCircuitBlock(id); selectNodes(["IN"]); return; }
      if (chord && key === "v") { e.preventDefault(); const fresh = pasteCircuitBlockAfter(id || "IN"); if (fresh) selectNodes([fresh]); return; }
      if (chord && id && key === "d" && canDeleteChip(n)) { e.preventDefault(); const fresh = duplicateCircuitBlock(id); if (fresh) selectNodes([fresh]); return; }
      if (chord && id && key === "p" && canDeleteChip(n)) { e.preventDefault(); parkCircuitBlock(id); return; }
      if (e.key === "Delete" || e.key === "Backspace") {
        const id = selectedRef.current;
        const n = id ? useBoardStore.getState().nodes[id] : undefined;
        if (id && canDeleteChip(n)) {
          e.preventDefault();
          removeCircuitBlock(id);
          clearBoardFocus();
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
      spaceRef.current = false;
    };
  }, [active]);

  const onWheel = useCallback((e: WheelEvent<HTMLDivElement>) => {
    if (! wheelOwnsZoom(e.target)) {
      return;
    }
    e.preventDefault();
    const pane = paneRef.current;
    if (! pane) {
      return;
    }
    const panePt = clientToPane(pane, e.clientX, e.clientY);
    applyLiveCamera(zoomCamera(camLive.current, panePt.x, panePt.y, e.deltaY > 0 ? 0.92 : 1.08), true);
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
    const panePt = clientToPane(pane, bindX, bindY);
    const world = worldFromScreen(camLive.current, panePt.x, panePt.y);
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
      data-overlay={menu || portBrowserId ? "1" : undefined}
      onPointerDown={onPointerDown}
      onPointerMove={onPointerMove}
      onPointerUp={onPointerUp}
      onPointerCancel={() => {
        connectDragRef.current = null;
        dragRef.current = null;
        chipDragRef.current = null;
        chipDragGroupRef.current = null;
        lassoRef.current = null;
        setLasso(null);
        panRef.current = null;
                camGesture.current = false;
        paneRef.current?.querySelectorAll<HTMLElement>("[data-node-id]").forEach((el) => { el.style.transform = ""; });
        paintCablesNow();
      }}
      onPointerLeave={() => {
        if (boardHoverRef.current !== null) {
          boardHoverRef.current = null;
          paintFocus();
        }
      }}
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
        const panePt = clientToPane(pane, e.clientX, e.clientY);
        const world = worldFromScreen(camLive.current, panePt.x, panePt.y);
        const chipId = (e.target as HTMLElement).closest("[data-node-id]")?.getAttribute("data-node-id") ?? null;
        const hit = boardContextHit(chipId, world, useBoardStore.getState());
        if (hit.kind === "chip") {
          const cur = usePatchSelectionStore.getState();
          if (cur.nodeIds.includes(hit.id)) {
            publishSelection({ nodeIds: cur.nodeIds, primaryId: hit.id, origin: "circuit", intent: "mark" });
          } else {
            selectNodes([hit.id]);
          }
          setMenu({ kind: "chip", ...placeOverlay(pane, e.clientX, e.clientY, CHIP_MENU_BOX), id: hit.id });
          return;
        }
        if (hit.kind === "edge") {
          setMenu({ kind: "add", ...placeOverlay(pane, e.clientX, e.clientY, ADD_PICKER_BOX), afterId: hit.sourceId, targetId: hit.targetId });
          return;
        }
        setMenu({ kind: "pane", ...placeOverlay(pane, e.clientX, e.clientY, ADD_PICKER_BOX), world });
      }}
      onClick={(e) => {
        if (lassoCompletedRef.current) { lassoCompletedRef.current = false; return; }
        if (shouldCollapseChipDetail(e.target, useBindStore.getState().letter)) {
          const hit = (document.elementFromPoint(e.clientX, e.clientY) as HTMLElement | null)
            ?? (e.target as HTMLElement);
          if (! hit.closest("[data-node-id], [data-board-control], .nk-ctx")) {
            useChipViewStore.getState().collapseAll();
            clearBoardFocus();
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
        active={active && circuitPaintActive("assemble", size.w, size.h)}
      />
      <BoardNavigator graph={graph} onFit={() => applyLiveCamera(fitBoardCamera(graph, size), true)}
        onArrange={() => { void layoutBoard("COMPACT", size, {force:true}); }}
        onFind={(id) => {
          const n=nodes[id]; if(!n)return;
          selectNodes([id]);
          applyLiveCamera(fitCamera([n],size),true);
        }} />
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
          n.role === "io" ? (
            <BoardTerminal
              key={n.id}
              node={n}
              ports={portsByNode[n.id] ?? []}
              selected={selection.includes(n.id)}
              onInspect={() => inspectChip(n.id, n.type)}
            />
          ) : (
            <BoardChip
              key={n.id}
              node={n}
              ports={portsByNode[n.id] ?? []}
              selected={selection.includes(n.id)}
              bindOver={bindAim?.id === n.id}
              bindLocal={bindAim?.id === n.id ? bindAim.local : { x: 0, y: 0 }}
              onPorts={()=>{setMenu(null);setPortBrowserId(n.id);}}
              onInspect={() => inspectChip(n.id, n.type)}
            />
          )
        ))}
      </div>
      {active && portBrowserId && nodes[portBrowserId] ? <PortBrowser key={portBrowserId} nodeId={portBrowserId} graph={graph} onClose={()=>setPortBrowserId(null)} /> : null}
      {lasso ? (() => {
        const a = screenFromWorld(camLive.current, lasso.x0, lasso.y0);
        const b = screenFromWorld(camLive.current, lasso.x1, lasso.y1);
        return <div className="nk-board-lasso" style={{ left: Math.min(a.x, b.x), top: Math.min(a.y, b.y), width: Math.abs(a.x - b.x), height: Math.abs(a.y - b.y) }} />;
      })() : null}
      {menu?.kind === "pane" ? (
        <OsContextMenu left={menu.left} top={menu.top} title="Add unconnected module" onDismiss={() => setMenu(null)}>
          <OsAddPicker onPick={(type, args) => {
            placeRef.current = menu.world;
            const id = addCircuitBlock(type, args);
            if (id) selectNodes([id]);
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
            <OsMenuItem onClick={() => { cutCircuitBlock(menu.id); selectNodes(["IN"]); setMenu(null); }}>Cut</OsMenuItem>
            <OsMenuItem onClick={() => { const id = duplicateCircuitBlock(menu.id); if (id) selectNodes([id]); setMenu(null); }}>Duplicate</OsMenuItem>
            <OsMenuItem onClick={() => { parkCircuitBlock(menu.id); setMenu(null); }}>Park</OsMenuItem>
          </> : null}
          <OsMenuItem onClick={() => { const id = pasteCircuitBlockAfter(menu.id); if (id) selectNodes([id]); setMenu(null); }}>Paste after</OsMenuItem>
          <OsMenuItem onClick={() => {
            const n = useBoardStore.getState().nodes[menu.id];
            if (n) {
              inspectChip(n.id, n.type);
            }
            setMenu(null);
          }}>Inspect</OsMenuItem>
          {menu != null && selection.length > 1 && selection.includes(menu.id) ? <>
            {() => {
              const flowExcluded = selection.filter((id) => isFlowBlockId(id)).length;
              const suffix = flowExcluded > 0 ? ` (${flowExcluded} blockiert)` : "";
              return (
                <>
                  <OsMenuItem onClick={() => { void setGroupMute(selection, true); setMenu(null); }}>Mute Gruppe{suffix}</OsMenuItem>
                  <OsMenuItem onClick={() => { void setGroupMute(selection, false); setMenu(null); }}>Gruppe entstummen{suffix}</OsMenuItem>
                  <OsMenuItem onClick={() => { void setGroupSolo(selection, true); setMenu(null); }}>Solo Gruppe{suffix}</OsMenuItem>
                  <OsMenuItem onClick={() => { void setGroupSolo(selection, false); setMenu(null); }}>Solo aufheben{suffix}</OsMenuItem>
                </>
              );
            }}()
          </> : null}
          {canDeleteChip(nodes[menu.id]) ? (
            <OsMenuItem danger onClick={() => {
              removeCircuitBlock(menu.id);
              clearBoardFocus();
              setMenu(null);
            }}>Delete</OsMenuItem>
          ) : null}
        </OsContextMenu>
      ) : null}
      {menu?.kind === "add" ? (
        <OsContextMenu left={menu.left} top={menu.top} title={menu.targetId ? `Insert ${menu.afterId} → ${menu.targetId}` : `Insert after ${menu.afterId}`} onDismiss={() => { setMenu(null); setInsertError(""); }}>
          <OsAddPicker onPick={async (type, args) => {
            const edge = menu.targetId ? Object.values(useBoardStore.getState().edges).find((e) =>
              e.sourceNodeId === menu.afterId && e.targetNodeId === menu.targetId && e.kind === "audio") : null;
            const insertionPoint = edge ? routeMidpoint(edgeRoute(useBoardStore.getState(), edge)) : null;
            placeRef.current = insertionPoint ?? { afterId: menu.afterId };
            const id = menu.targetId
              ? await insertCircuitBlockOnEdge(menu.afterId, menu.targetId, type, args)
              : insertCircuitBlockAfter(menu.afterId, type, args);
            if (!id) { placeRef.current = null; setInsertError("This module cannot be inserted on this connection. Choose an audio block or another connection."); return; }
            if (id) selectNodes([id]);
            setInsertError("");
            setMenu(null);
          }} />
          {insertError ? <div className="nk-board-insert-error" role="alert">{insertError}</div> : null}
        </OsContextMenu>
      ) : null}
    </div>
  );
}
