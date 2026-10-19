import type { ReactNode } from "react";
import { frameCorners, framePoints } from "../../theme/chromeSpec";
import { icBodyRect } from "../chipMetrics";

/** Shared IC identity. Selection changes paint only, never layout. */
export function ChipChrome({ box, cut, title, typecode, lamp, controls, inspect, hidden, selected, inAccent, pads }: {
  box: { w: number; h: number }; cut: number; title: string; typecode?: string;
  lamp?: ReactNode; controls?: ReactNode; inspect?: ReactNode; hidden: boolean;
  selected: boolean; inAccent: boolean; pads: boolean;
}) {
  const body = icBodyRect(box);
  const stroke = selected ? "var(--nk-accent)" : inAccent ? "var(--nk-cyan)" : "var(--nk-ink-muted)";
  return <>
    <div className="nk-chip-fill" style={hidden ? { opacity: 0.25 } : undefined} />
    {pads ? <div className="nk-chip-pads" aria-hidden style={hidden ? { visibility: "hidden" } : undefined} /> : null}
    <div className="nk-chip-headband" style={hidden ? { visibility: "hidden" } : undefined}>
      {lamp}
      <span className="nk-chip-title" title={title}>{title}</span>
    </div>
    <div className="nk-chip-subheader" style={hidden ? { visibility: "hidden" } : undefined}>
      <span className="nk-chip-typecode" title={typecode}>{typecode}</span>
      <div className="nk-chip-controls">{controls}{inspect}</div>
    </div>
    <svg className="nk-chip-frame" width={box.w} height={box.h} viewBox={`0 0 ${box.w} ${box.h}`} aria-hidden>
      <g transform={`translate(${body.x} ${body.y})`}>
        <g className="nk-chip-facets" data-material="gunmetal">
          <path d={`M ${cut} 0 H ${body.w} L ${body.w - 4} 4 H ${cut + 2} L 4 ${cut + 2} V ${body.h - 4} L 0 ${body.h} V ${cut} Z`} fill="rgb(255 255 255 / .12)" />
          <path d={`M ${body.w} 0 V ${body.h - cut} L ${body.w - cut} ${body.h} H 0 L 4 ${body.h - 4} H ${body.w - cut - 2} L ${body.w - 4} ${body.h - cut - 2} V 4 Z`} fill="rgb(0 0 0 / .6)" />
        </g>
        <polygon points={framePoints(body.w, body.h, cut)} fill="none" stroke={stroke} strokeWidth={selected ? 1.8 : 1} />
        {frameCorners(body.w, body.h, cut).map((d, i) => <path key={i} d={d} fill="none" stroke={selected ? stroke : "var(--nk-ink-soft)"} strokeWidth="2" />)}
      </g>
    </svg>
  </>;
}
