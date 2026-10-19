import { chipSpec } from "../chipSpec";

const VIEW_H = 40;
const WEST = 18;
const EAST = 82;

export type IoJack = { id: string; x: number; y: number; east: boolean };

export type IoFaceModel = {
  ins: string[];
  outs: string[];
};

export function ioFaceModel(chipType: string, args: Record<string, string> = {}): IoFaceModel {
  const spec = chipSpec(chipType, args);
  return { ins: spec.audioIns, outs: spec.audioOuts };
}

function spread(count: number, index: number): number {
  if (count <= 1) {
    return VIEW_H * 0.5;
  }
  return 10 + (20 * index) / (count - 1);
}

/** ChipSpec contacts as a schematic. Real jacks stay on the frame. */
export function ioJackDots(chipType: string, args: Record<string, string> = {}): IoJack[] {
  const m = ioFaceModel(chipType, args);
  const dots: IoJack[] = [];
  m.ins.forEach((id, i) => {
    dots.push({ id, x: WEST, y: spread(m.ins.length, i), east: false });
  });
  m.outs.forEach((id, i) => {
    dots.push({ id, x: EAST, y: spread(m.outs.length, i), east: true });
  });
  return dots;
}

export function ioJackPath(chipType: string, args: Record<string, string> = {}): string {
  const m = ioFaceModel(chipType, args);
  if (m.ins.length === 0 && m.outs.length === 0) {
    return `M${WEST} 20 L${EAST} 20`;
  }
  if (m.ins.length === 0) {
    return m.outs
      .map((_, i) => {
        const y = spread(m.outs.length, i);
        return `M${WEST} ${y.toFixed(2)} L${EAST} ${y.toFixed(2)}`;
      })
      .join(" ");
  }
  if (m.outs.length === 0) {
    return m.ins
      .map((_, i) => {
        const y = spread(m.ins.length, i);
        return `M${WEST} ${y.toFixed(2)} L${EAST} ${y.toFixed(2)}`;
      })
      .join(" ");
  }
  const parts: string[] = [];
  m.ins.forEach((_, i) => {
    const y1 = spread(m.ins.length, i);
    m.outs.forEach((__, j) => {
      const y2 = spread(m.outs.length, j);
      parts.push(`M${WEST} ${y1.toFixed(2)} L${EAST} ${y2.toFixed(2)}`);
    });
  });
  return parts.join(" ");
}

