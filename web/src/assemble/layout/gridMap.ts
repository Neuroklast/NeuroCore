import { BOARD_GRID } from "../grid";
import type { Cell, LayoutNode, Pt } from "./types";

export const H = 1;
export const V = 2;

export class GridMap {
  minC = 0;
  minR = 0;
  maxC = 0;
  maxR = 0;
  solid = new Set<string>();
  halo = new Set<string>();
  cable = new Map<string, number>();
  turns = new Set<string>();
  entryStubs = new Set<string>();
  exitStubs = new Set<string>();
  /** Cell → edge id. Foreign cables may not enter another port's runway. */
  lane = new Map<string, string>();

  key(c: number, r: number): string {
    return `${c},${r}`;
  }

  inBounds(c: number, r: number): boolean {
    return c >= this.minC && c <= this.maxC && r >= this.minR && r <= this.maxR;
  }

  expandTo(c: number, r: number, pad = 8): void {
    this.minC = Math.min(this.minC, c - pad);
    this.minR = Math.min(this.minR, r - pad);
    this.maxC = Math.max(this.maxC, c + pad);
    this.maxR = Math.max(this.maxR, r + pad);
  }

  markNode(n: LayoutNode, x: number, y: number): void {
    const c0 = Math.floor(x / BOARD_GRID);
    const r0 = Math.floor(y / BOARD_GRID);
    const c1 = Math.ceil((x + n.w) / BOARD_GRID) - 1;
    const r1 = Math.ceil((y + n.h) / BOARD_GRID) - 1;
    this.expandTo(c0, r0);
    this.expandTo(c1, r1);
    for (let c = c0; c <= c1; c += 1) {
      for (let r = r0; r <= r1; r += 1) {
        this.solid.add(this.key(c, r));
      }
    }
  }

  finishHalo(): void {
    const dirs = [[1, 0], [-1, 0], [0, 1], [0, -1]];
    const ring: string[] = [];
    for (const k of this.solid) {
      const [cs, rs] = k.split(",");
      const c = Number(cs);
      const r = Number(rs);
      for (const [dc, dr] of dirs) {
        const nk = this.key(c + dc, r + dr);
        if (! this.solid.has(nk)) {
          ring.push(nk);
        }
      }
    }
    for (const nk of ring) {
      this.halo.add(nk);
      const stub = this.entryStubs.has(nk) || this.exitStubs.has(nk);
      if (! stub) {
        this.solid.add(nk);
      }
    }
  }

  markEntryStub(c: number, r: number): void {
    this.entryStubs.add(this.key(c, r));
  }

  markExitStub(c: number, r: number): void {
    this.exitStubs.add(this.key(c, r));
  }

  /** Two cells east of an out (or west of an in) belong only to that net. */
  reserveRunway(edgeId: string, cell: Cell, toward: 1 | -1, cells = 2): void {
    for (let i = 0; i < cells; i += 1) {
      const c = cell.c + toward * i;
      const k = this.key(c, cell.r);
      if (! this.lane.has(k)) {
        this.lane.set(k, edgeId);
      }
      this.expandTo(c, cell.r, 2);
    }
  }

  foreignLane(c: number, r: number, edgeId: string): boolean {
    const owner = this.lane.get(this.key(c, r));
    return owner != null && owner !== edgeId;
  }

  occupy(cells: Cell[], turns: Cell[]): void {
    for (let i = 1; i < cells.length; i += 1) {
      const a = cells[i - 1]!;
      const b = cells[i]!;
      const bit = a.c === b.c ? V : H;
      const k = this.key(b.c, b.r);
      this.cable.set(k, (this.cable.get(k) ?? 0) | bit);
    }
    if (cells[0]) {
      const a = cells[0];
      const b = cells[1] ?? cells[0];
      const bit = a.c === b.c ? V : H;
      const k = this.key(a.c, a.r);
      this.cable.set(k, (this.cable.get(k) ?? 0) | bit);
    }
    for (const t of turns) {
      this.turns.add(this.key(t.c, t.r));
    }
  }

  cellCenter(c: number, r: number): Pt {
    return { x: c * BOARD_GRID + BOARD_GRID * 0.5, y: r * BOARD_GRID + BOARD_GRID * 0.5 };
  }
}

export function portOutCell(nodeX: number, nodeW: number, portY: number): Cell {
  return { c: Math.floor((nodeX + nodeW) / BOARD_GRID), r: Math.floor(portY / BOARD_GRID) };
}

export function portInCell(nodeX: number, portY: number): Cell {
  return { c: Math.floor(nodeX / BOARD_GRID) - 1, r: Math.floor(portY / BOARD_GRID) };
}
