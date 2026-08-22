export type LayoutMode = "ARRANGE" | "COMPACT" | "REROUTE";

export type LayoutPort = { id: string; y: number };

export type LayoutNode = {
  id: string;
  x?: number;
  y?: number;
  w: number;
  h: number;
  ins: LayoutPort[];
  outs: LayoutPort[];
  /** Named bus / "main" / "mod". Compact packs each rail as its own row. */
  rail?: string;
};

export type LayoutEdge = {
  id: string;
  source: string;
  target: string;
  fromJack: string;
  toJack: string;
};

export type LayoutView = { w: number; h: number };

export type LayoutRequest = {
  type: LayoutMode;
  nodes: LayoutNode[];
  edges: LayoutEdge[];
  view?: LayoutView;
};

export type LayoutResult = {
  nodes: Record<string, { x: number; y: number; w: number; h: number }>;
  edgePaths: Record<string, string>;
};

export type Pt = { x: number; y: number };
export type Cell = { c: number; r: number };
export type Dir = 0 | 1 | 2 | 3;

export const DIR_E = 0 as const;
export const DIR_N = 1 as const;
export const DIR_W = 2 as const;
export const DIR_S = 3 as const;

export const STEP_COST = 10;
export const TURN_COST = 100;
export const HALO_COST = 50;
export const CROSS_COST = 500;
export const INF = 1e12;
