export type ChipDrag = { id: string; x: number; y: number };

export const chipDragRef: { current: ChipDrag | null } = { current: null };

export function nodeWithDrag<T extends { id: string; x: number; y: number }>(
  node: T,
  drag: ChipDrag | null,
): T {
  if (! drag || drag.id !== node.id) {
    return node;
  }
  return { ...node, x: drag.x, y: drag.y };
}

export function chipDragTranslate(
  drag: ChipDrag,
  origin: { x: number; y: number },
): string {
  return `translate(${drag.x - origin.x}px, ${drag.y - origin.y}px)`;
}

export function applyChipDragStyle(
  el: { style: { transform: string } },
  drag: ChipDrag | null,
  origin: { x: number; y: number },
  nodeId: string,
): void {
  if (drag && drag.id === nodeId) {
    el.style.transform = chipDragTranslate(drag, origin);
    return;
  }
  el.style.transform = "";
}
