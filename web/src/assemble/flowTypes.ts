/** Local stand-in for the old React Flow node/edge shapes used by tests and ELK adapters. */

export const Position = {
  Left: "left",
  Top: "top",
  Right: "right",
  Bottom: "bottom",
} as const;

export type Position = (typeof Position)[keyof typeof Position];

export type Node<T = unknown> = {
  id: string;
  type?: string;
  position: { x: number; y: number };
  data: T;
  width?: number;
  height?: number;
  style?: { width?: number; height?: number };
  sourcePosition?: Position;
  targetPosition?: Position;
  draggable?: boolean;
  dragHandle?: string;
  measured?: { width?: number; height?: number };
};

export type Edge<T = unknown> = {
  id: string;
  source: string;
  target: string;
  sourceHandle?: string | null;
  targetHandle?: string | null;
  className?: string;
  data?: T;
};
