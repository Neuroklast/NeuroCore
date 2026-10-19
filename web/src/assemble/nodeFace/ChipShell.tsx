import type { CSSProperties, HTMLAttributes, ReactNode } from "react";
import { chipExpandOffset, DETAIL_HIT, frameCorners, framePoints, headbandEndPad } from "../../theme/chromeSpec";
import { chipFootLine, chipFootPeak } from "../chipFace";
import { useHostStore } from "../../store/hostStore";
import type { FaceKind } from "./faceKind";

type ShellProps = {
  nodeId: string;
  role: "io" | "chip";
  selected: boolean;
  kind: FaceKind;
  depth: 1 | 2 | 3;
  audible: boolean;
  title: string;
  typecode?: string;
  frameCut: number;
  inAccent: boolean;
  bindOpen: boolean;
  showExpand: boolean;
  inspectOpen: boolean;
  onInspect?: () => void;
  headerStart?: ReactNode;
  headerEnd?: ReactNode;
  bodyStart?: ReactNode;
  children?: ReactNode;
  style: CSSProperties;
} & Omit<HTMLAttributes<HTMLDivElement>, "children" | "style" | "role" | "title">;

export function ChipShell({
  nodeId,
  role,
  selected,
  kind,
  depth,
  audible,
  title,
  typecode,
  frameCut,
  inAccent,
  bindOpen,
  showExpand,
  inspectOpen,
  onInspect,
  headerStart,
  headerEnd,
  bodyStart,
  children,
  style,
  ...rest
}: ShellProps) {
  const expandAt = chipExpandOffset();
  const stroke = selected ? "var(--nk-accent)" : inAccent ? "var(--nk-cyan)" : "var(--nk-ink-muted)";
  const corner = selected ? "var(--nk-accent)" : inAccent ? "var(--nk-cyan)" : "var(--nk-ink-soft)";
  const w = typeof style.width === "number" ? style.width : Number.parseFloat(String(style.width));
  const h = typeof style.height === "number" ? style.height : Number.parseFloat(String(style.height));

  return (
    <div
      className={role === "io" ? "nk-chip-io nk-board-chip" : "nk-chip nk-board-chip"}
      data-node-id={nodeId}
      data-bind-node={nodeId}
      data-face={kind}
      data-depth={depth}
      data-audible={audible ? "on" : "off"}
      style={{
        ...style,
        opacity: audible ? 1 : 0.4,
        overflow: "visible",
        ...({ "--nk-chip-cut": `${frameCut}px` } as CSSProperties),
      }}
      {...rest}
    >
      <div className="nk-chip-fill" style={bindOpen ? { opacity: 0.25 } : undefined} />
      {children}
      <div
        className="nk-chip-headband"
        style={{
          ...(bindOpen ? { visibility: "hidden" as const } : {}),
          ...({ "--nk-head-end": `${headbandEndPad(showExpand)}px` } as CSSProperties),
        }}
      >
        {headerStart}
        <span className="nk-chip-title min-w-0 flex-1 truncate text-ink">
          {title}
        </span>
        {headerEnd}
      </div>
      <svg className="nk-chip-frame" width={w} height={h} viewBox={`0 0 ${w} ${h}`} aria-hidden>
        <polygon
          points={framePoints(w, h, frameCut)}
          fill="none"
          stroke={stroke}
          strokeWidth={selected ? "2.2" : "1.4"}
        />
        {frameCorners(w, h, frameCut).map((d, i) => (
          <path key={i} d={d} fill="none" stroke={corner} strokeWidth="2.7" />
        ))}
      </svg>
      <div className="nk-chip-body" style={{ pointerEvents: "none", ...(bindOpen ? { visibility: "hidden" as const } : {}) }}>
        {role === "chip" && typecode ? (
          <span className="nk-chip-typecode">{typecode}</span>
        ) : null}
        {bodyStart}
      </div>
      <ChipFoot nodeId={nodeId} bindOpen={bindOpen} />
      {showExpand && ! bindOpen ? (
        <button
          type="button"
          className="nk-chip-expand nodrag nopan"
          data-chip-keep-open=""
          aria-expanded={inspectOpen || bindOpen}
          style={{
            position: "absolute",
            top: expandAt.top,
            right: expandAt.right,
            zIndex: 6,
            width: expandAt.size,
            height: expandAt.size,
            minWidth: DETAIL_HIT,
            minHeight: DETAIL_HIT,
          }}
          onClick={(e) => { e.stopPropagation(); onInspect?.(); }}
        >
          {inspectOpen ? "▴" : "▾"}
        </button>
      ) : null}
    </div>
  );
}

function ChipFoot({ nodeId, bindOpen }: { nodeId: string; bindOpen: boolean }) {
  const peak = useHostStore((s) => s.clips[nodeId] ?? 0);
  return (
    <div className="nk-chip-foot" title={chipFootLine(nodeId, peak)} style={bindOpen ? { visibility: "hidden" } : undefined}>
      <span>{nodeId}</span>
      <span>{chipFootPeak(peak)}</span>
    </div>
  );
}
