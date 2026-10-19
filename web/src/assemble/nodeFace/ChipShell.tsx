import type { CSSProperties, HTMLAttributes, ReactNode } from "react";
import { ChipChrome } from "./ChipChrome";
import { chipFootLine, chipFootPeak } from "../chipFace";
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
  const w = typeof style.width === "number" ? style.width : Number.parseFloat(String(style.width));
  const h = typeof style.height === "number" ? style.height : Number.parseFloat(String(style.height));
  const box = { w: Number.isFinite(w) ? w : 0, h: Number.isFinite(h) ? h : 0 };

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
        overflow: "visible",
        ...({ "--nk-chip-cut": `${frameCut}px` } as CSSProperties),
      }}
      {...rest}
    >
      <ChipChrome
        box={box} cut={frameCut} title={title} typecode={typecode}
        lamp={headerStart} controls={headerEnd} hidden={bindOpen}
        selected={selected} inAccent={inAccent} pads={role === "chip"}
        inspect={showExpand && !bindOpen ? (
          <button type="button" className="nk-chip-expand nodrag nopan"
            data-chip-keep-open="" aria-label={`Inspect ${title}`}
            aria-expanded={inspectOpen}
            onClick={e => { e.stopPropagation(); onInspect?.(); }}>
            {inspectOpen ? "▴" : "▾"}
          </button>
        ) : null}
      />
      {children}
      <div className="nk-chip-body" style={{ pointerEvents: "none", ...(bindOpen ? { visibility: "hidden" as const } : {}) }}>
        {bodyStart}
      </div>
      <ChipFoot nodeId={nodeId} nodeType={nodeType} bindOpen={bindOpen} />

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
