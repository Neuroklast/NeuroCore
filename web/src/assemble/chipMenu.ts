/** Node context: insert, inspect, delete. Mute/solo live on the chip face. */
export const CHIP_MENU = ["insertAfter", "inspect", "delete"] as const;

export type ChipMenuAction = (typeof CHIP_MENU)[number];

export function chipMenuActions(): ChipMenuAction[] {
  return [...CHIP_MENU];
}