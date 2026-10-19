/** Graph zoom is only for wheels that are not over a Circuit overlay. */
export function wheelOwnsZoom(target: { closest?: (sel: string) => unknown } | EventTarget | null): boolean {
  if (target == null || typeof target !== "object") {
    return true;
  }
  const el = target as { closest?: (sel: string) => unknown };
  return typeof el.closest !== "function"
    || el.closest(".nk-ctx, [data-add-picker], [data-board-control]") == null;
}
