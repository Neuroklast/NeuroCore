import type { BoardNode } from "./boardModel";
/** The board is a single material surface under the actual circuit, not a group. */
export function drawBoardSubstrate(
  ctx: CanvasRenderingContext2D,
  nodes: BoardNode[],
  ink: string,
  surface: string,
) {
  if (!nodes.length) return;
  const x = Math.min(...nodes.map((n) => n.x)) - 40,
    y = Math.min(...nodes.map((n) => n.y)) - 40;
  const right = Math.max(...nodes.map((n) => n.x + n.w)) + 40,
    bottom = Math.max(...nodes.map((n) => n.y + n.h)) + 40;
  ctx.save();
  ctx.shadowBlur = 0;
  ctx.setLineDash([]);
  ctx.beginPath();
  ctx.moveTo(x + 16, y);
  ctx.lineTo(right - 16, y);
  ctx.lineTo(right, y + 16);
  ctx.lineTo(right, bottom - 16);
  ctx.lineTo(right - 16, bottom);
  ctx.lineTo(x + 16, bottom);
  ctx.lineTo(x, bottom - 16);
  ctx.lineTo(x, y + 16);
  ctx.closePath();
  ctx.fillStyle = surface;
  ctx.globalAlpha = 0.9;
  ctx.fill();
  ctx.strokeStyle = ink;
  ctx.lineWidth = 2;
  ctx.globalAlpha = 0.22;
  ctx.stroke();
  ctx.globalAlpha = 0.1;
  ctx.lineWidth = 1;
  ctx.strokeRect(x + 12, y + 12, right - x - 24, bottom - y - 24);
  for (const [cx, cy] of [
    [x + 22, y + 22],
    [right - 22, y + 22],
    [x + 22, bottom - 22],
    [right - 22, bottom - 22],
  ]) {
    ctx.globalAlpha = 0.28;
    ctx.lineWidth = 3;
    ctx.beginPath();
    ctx.arc(cx!, cy!, 6, 0, Math.PI * 2);
    ctx.stroke();
    ctx.globalAlpha = 0.65;
    ctx.fillStyle = surface;
    ctx.beginPath();
    ctx.arc(cx!, cy!, 3, 0, Math.PI * 2);
    ctx.fill();
  }
  ctx.restore();
}
