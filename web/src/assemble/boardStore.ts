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
  hydrate: (ast: AstDocument | null, sidechainOn?: boolean, keepXy?: boolean) => void;
  setCamera: (camera: BoardCamera) => void;
  setEdges: (edges: Record<string, BoardEdge>) => void;
  moveNode: (id: string, x: number, y: number) => void;
  applyLayout: (
    placed: Record<string, { x: number; y: number; w: number; h: number }>,
    routes: Record<string, Array<{ x: number; y: number }>>,
  ) => void;
};

const empty: BoardGraph = { nodes: {}, ports: {}, edges: {} };

export const useBoardStore = create<BoardState>((set) => ({
  ...empty,
  camera: { tx: 32, ty: 32, scale: 1 },
  userMoved: false,
  hydrate: (ast, sidechainOn = false, keepXy = false) => {
    if (! ast) {
      set({ ...empty, userMoved: false });
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
    set({ ...next, userMoved: false });
  },
  setCamera: (camera) => set({ camera }),
  setEdges: (edges) => set({ edges }),
  moveNode: (id, x, y) => set((s) => {
    const n = s.nodes[id];
    if (! n || n.locked) {
      return s;
    }
    return {
      userMoved: true,
      nodes: { ...s.nodes, [id]: { ...n, x: snapToGrid(x), y: snapToGrid(y) } },
    };
  }),
  applyLayout: (placed, routes) => set((s) => ({
    ...applyPlaced(s, placed, routes),
  })),
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
