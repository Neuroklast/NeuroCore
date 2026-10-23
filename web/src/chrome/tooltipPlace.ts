export const TOOLTIP_PAD = 4;
export const TOOLTIP_GAP = 6;

export type Box = { x: number; y: number; w: number; h: number };

export function scaleRect(
  host: { left: number; top: number; width: number; height: number },
  el: { left: number; top: number; width: number; height: number },
  viewW: number,
  viewH: number,
): { trigger: Box; view: { w: number; h: number } } {
  const sx = host.width > 0 ? viewW / host.width : 1;
  const sy = host.height > 0 ? viewH / host.height : 1;
  return {
    trigger: {
      x: (el.left - host.left) * sx,
      y: (el.top - host.top) * sy,
      w: el.width * sx,
      h: el.height * sy,
    },
    view: { w: viewW, h: viewH },
  };
}

export function placeTooltip(
  trigger: { x: number; y: number; w: number; h: number },
  size: { w: number; h: number },
  view: { w: number; h: number },
): { x: number; y: number } {
  let x = trigger.x + trigger.w / 2 - size.w / 2;
  let y = trigger.y - TOOLTIP_GAP - size.h;
  if (y < TOOLTIP_PAD) {
    y = trigger.y + trigger.h + TOOLTIP_GAP;
  }
  if (y + size.h > view.h - TOOLTIP_PAD) {
    y = Math.max(TOOLTIP_PAD, view.h - TOOLTIP_PAD - size.h);
  }
  x = Math.min(view.w - TOOLTIP_PAD - size.w, Math.max(TOOLTIP_PAD, x));
  return { x, y };
}
