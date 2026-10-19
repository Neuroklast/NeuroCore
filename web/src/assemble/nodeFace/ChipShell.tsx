import type { CSSProperties, HTMLAttributes, ReactNode } from "react";
import { chipExpandOffset, DETAIL_HIT, headbandEndPad } from "../../theme/chromeSpec";
import { chipFootLine, chipFootPeak } from "../chipFace";
import { icBodyRect } from "../chipMetrics";
import { nodeMeter } from "../nodeMeter";
import { useHostStore } from "../../store/hostStore";
import type { FaceKind } from "./faceKind";

type ShellProps = {
  nodeId: string;
  nodeType?: string;
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
  nodeType,
  role,
  selected,
  kind,
  depth,
  audible,
  title,
  typecode,
  frameCut: _frameCut,
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
  const w = typeof style.width === "number" ? style.width : Number.parseFloat(String(style.width));
  const h = typeof style.height === "number" ? style.height : Number.parseFloat(String(style.height));
  const body = icBodyRect({ w: Number.isFinite(w) ? w : 0, h: Number.isFinite(h) ? h : 0 });

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
        ...({ "--nk-chip-cut": `${_frameCut}px` } as CSSProperties),
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
        {role === "chip" && typecode ? (
          <span className="nk-chip-typecode">{typecode}</span>
        ) : null}
        {headerEnd}
      </div>
      <svg className="nk-chip-frame" width={w} height={h} viewBox={`0 0 ${w} ${h}`} aria-hidden>
        <rect
          x={body.x}
          y={body.y}
          width={body.w}
          height={body.h}
          rx="2"
          ry="2"
          fill="none"
          stroke={stroke}
          strokeWidth={selected ? "1.6" : "1"}
        />
      </svg>
      <div className="nk-chip-body" style={{ pointerEvents: "none", ...(bindOpen ? { visibility: "hidden" as const } : {}) }}>
        {bodyStart}
      </div>
      <ChipFoot nodeId={nodeId} nodeType={nodeType} bindOpen={bindOpen} />
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

function ChipFoot({ nodeId, nodeType, bindOpen }: { nodeId: string; nodeType?: string; bindOpen: boolean }) {
  const peak = useHostStore((s) => nodeMeter({ id: nodeId, type: nodeType, clips: s.clips }).peak);
  return (
    <div className="nk-chip-foot" title={chipFootLine(nodeId, peak)} style={bindOpen ? { visibility: "hidden" } : undefined}>
      <span>{nodeId}</span>
      <span>{chipFootPeak(peak)}</span>
    </div>
  );
}
