import { create } from "zustand";
import type { AstDocument } from "../bridge/ast";
import {
  applyPlaced,
  hydrateBoard,
  type BoardCamera,
  type BoardEdge,
  type BoardGraph,
  type BoardNode,
  type BoardPort,
} from "./boardModel";
import { snapToGrid } from "./grid";

export type BoardState = BoardGraph & {
  camera: BoardCamera;
  userMoved: boolean;
  layoutEpoch: number;
  hydrate: (ast: AstDocument | null, sidechainOn?: boolean, keepXy?: boolean) => void;
  setCamera: (camera: BoardCamera) => void;
  setEdges: (edges: Record<string, BoardEdge>) => void;
  moveNode: (id: string, x: number, y: number) => void;
  applyLayout: (
    placed: Record<string, { x: number; y: number; w: number; h: number }>,
    routes: Record<string, Array<{ x: number; y: number }>>,
    epoch?: number,
  ) => void;
};

const empty: BoardGraph = { nodes: {}, ports: {}, edges: {} };

export const useBoardStore = create<BoardState>((set) => ({
  ...empty,
  camera: { tx: 32, ty: 32, scale: 1 },
  userMoved: false,
  layoutEpoch: 0,
  hydrate: (ast, sidechainOn = false, keepXy = false) => {
    const epoch = useBoardStore.getState().layoutEpoch + 1;
    if (! ast) {
      set({ ...empty, userMoved: false, layoutEpoch: epoch });
      return;
    }
    const next = hydrateBoard(ast, sidechainOn);
    if (keepXy) {
      const prev = useBoardStore.getState().nodes;
      for (const [id, n] of Object.entries(next.nodes)) {
        const p = prev[id];
        if (p) {
          next.nodes[id] = { ...n, x: p.x, y: p.y };
        }
      }
      set({ ...next, userMoved: true });
      return;
    }
    set({ ...next, userMoved: false, layoutEpoch: epoch });
  },
  setCamera: (camera) => set({ camera }),
  setEdges: (edges) => set({ edges }),
  moveNode: (id, x, y) => set((s) => {
    const n = s.nodes[id];
    if (! n || n.locked) {
      return s;
    }
    const edges = { ...s.edges };
    for (const [eid, e] of Object.entries(edges)) {
      if (e.sourceNodeId === id || e.targetNodeId === id) {
        edges[eid] = { ...e, route: [] };
      }
    }
    return {
      userMoved: true,
      nodes: { ...s.nodes, [id]: { ...n, x: snapToGrid(x), y: snapToGrid(y) } },
      edges,
    };
  }),
  applyLayout: (placed, routes, epoch) => set((s) => {
    if (epoch != null && epoch !== s.layoutEpoch) {
      return s;
    }
    return applyPlaced(s, placed, routes);
  }),
}));

export function boardNodes(): BoardNode[] {
  return Object.values(useBoardStore.getState().nodes);
}

export function boardPorts(): BoardPort[] {
  return Object.values(useBoardStore.getState().ports);
}

export function boardEdges(): BoardEdge[] {
  return Object.values(useBoardStore.getState().edges);
}
