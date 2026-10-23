import { BOARD_GRID } from "./grid";

/** Captions are bounded; full parameter names belong to Inspect/connection search. */
export const COMPACT_CHIP_MAX_W = 448;
export const PORT_BANK_CAPTION_W = 84;

/** Allocate stable IDs to two rails. A connected port never disappears. */
export function horizontalPortBanks<T>(ports: readonly T[]): { south: T[]; north: T[] } {
  const southCount = ports.length <= 4 ? ports.length : Math.max(4, Math.ceil(ports.length / 2));
  return { south: ports.slice(0, southCount), north: ports.slice(southCount) };
}

export function portBankMinWidth(count: number): number {
  const banks = horizontalPortBanks(Array.from({length:count},(_,i)=>i));
  return (Math.max(banks.south.length,banks.north.length)+1)*BOARD_GRID;
}
