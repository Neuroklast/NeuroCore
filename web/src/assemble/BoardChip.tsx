import type { CSSProperties } from "react";
import { CHIP_CUT, chipExpandOffset, DETAIL_HIT, frameCorners, framePoints, headbandEndPad } from "../theme/chromeSpec";
import { useChipViewStore } from "../store/expandStore";
import { useHostStore } from "../store/hostStore";
import { BindDropPad, BindRail } from "./BindRail";
import { chipExpandAction } from "./boardEdit";
import {
  bindRailVisible,
  chipFootLine,
  chipFootPeak,
  clipWarnMark,
  closedChipChrome,
  overloadLabel,
  portBracket,
} from "./chipFace";
import { bindFace, chipChromeVars, ioBodyInset } from "./chipLayout";
import { collapsedFace, paintedBindKeys } from "./chipSpec";
import { isFlowBlockId } from "./muteSolo";
import { toggleChipMute, toggleChipSolo } from "./muteSoloApply";
import { BOARD_HIT, portLocal, type BoardNode, type BoardPort } from "./boardModel";

export function BoardChip({
  node,
  ports,
  selected,
  focus,
  bindOver = false,
  bindLocal = { x: 0, y: 0 },
  onInspect,
}: {
  node: BoardNode;
  ports: BoardPort[];
  selected: boolean;
  focus?: "off" | "soft" | "sharp";
  bindOver?: boolean;
  bindLocal?: { x: number; y: number };
  onInspect?: () => void;
}) {
  const isMuted = useChipViewStore((s) => s.isMuted(node.id));
  const isSoloed = useChipViewStore((s) => s.isSoloed(node.id));
  const isAudible = useChipViewStore((s) => s.isAudible(node.id));
  const peak = useHostStore((s) => s.clips[node.id] ?? 0);
  const peakL = useHostStore((s) => s.clipsL[node.id] ?? peak);
  const peakR = useHostStore((s) => s.clipsR[node.id] ?? peak);
  const inspectOpen = useHostStore((s) => s.inspectId === node.id);
  const chrome = closedChipChrome(node.type, node.id);
  const face = collapsedFace(node.type, node.args);
  const title = node.role === "io" ? node.label : face.title;
  const expandAt = chipExpandOffset();
  const canMs = ! isFlowBlockId(node.id) && node.role === "chip";
  const isIn = node.type === "in";
  const ioInset = node.role === "io" ? ioBodyInset() : null;
  const showExpand = chipExpandAction(node.role, node.type) === "inspect";
  const binds = node.role === "chip" ? paintedBindKeys(node.type, node.args) : [];
  const bindOpen = bindOver && node.role === "chip" && binds.length > 0;

  return (
    <div
      className={node.role === "io" ? "nk-chip-io nk-board-chip" : "nk-chip nk-board-chip"}
      data-node-id={node.id}
      data-bind-node={node.id}
      data-selected={selected ? "on" : "off"}
      data-focus={bindOpen ? "sharp" : (focus ?? "off")}
      data-io={node.role === "io" ? node.type : undefined}
      style={{
        position: "absolute",
        left: node.x,
        top: node.y,
        width: node.w,
        height: node.h,
        opacity: isAudible ? 1 : 0.4,
        overflow: "visible",
        ...({
          ...chipChromeVars(node.role),
          "--nk-chip-cut": `${CHIP_CUT}px`,
        } as CSSProperties),
        ...(ioInset
          ? {
            "--nk-label-col": `${ioInset.left}px`,
            "--nk-south-gap": `${ioInset.bottom}px`,
          } as CSSProperties
          : {}),
      }}
    >
      <div className="nk-chip-fill" style={bindOpen ? { opacity: 0.25 } : undefined} />
      {node.role === "chip" ? <div className="nk-chip-pads" aria-hidden /> : null}
      <div
        className="nk-chip-headband"
        style={{
          visibility: bindOpen ? "hidden" : "visible",
          ...({ "--nk-head-end": `${headbandEndPad(showExpand)}px` } as CSSProperties),
        }}
      >
        {node.role === "chip" ? (
          chrome.lamp === "clip" ? (
            <span className={`nk-clip-lamp${peak >= 1 ? " is-clip" : ""}`} aria-hidden />
          ) : chrome.lamp === "env" ? (
            <span className="nk-env-lamp" aria-hidden />
          ) : (
            <span className="nk-lfo-lamp nk-lfo-sine" aria-hidden />
          )
        ) : null}
        <span className="nk-chip-title min-w-0 flex-1 truncate text-ink">
          {title}
        </span>
      </div>
      <svg className="nk-chip-frame" width={node.w} height={node.h} viewBox={`0 0 ${node.w} ${node.h}`} aria-hidden>
        <polygon
          points={framePoints(node.w, node.h)}
          fill="none"
          stroke={selected || isIn ? "var(--nk-cyan)" : "var(--nk-ink-muted)"}
          strokeWidth="1.4"
        />
        {frameCorners(node.w, node.h).map((d, i) => (
          <path key={i} d={d} fill="none" stroke={selected || isIn ? "var(--nk-cyan)" : "var(--nk-ink-soft)"} strokeWidth="2.7" />
        ))}
      </svg>
      <div className="nk-chip-body" style={{ pointerEvents: "none", visibility: bindOpen ? "hidden" : "visible" }}>
        {node.role === "chip" ? (
          <span className="nk-chip-typecode">{face.code}</span>
        ) : null}
        {canMs ? (
          <div className="nk-chip-ms flex items-center gap-1" style={{ pointerEvents: "auto" }}>
            <button type="button" className={`nk-ms nodrag nopan ${isMuted ? "is-on" : ""}`} onClick={(e) => { e.stopPropagation(); toggleChipMute(node.id); }}>M</button>
            <button type="button" className={`nk-ms nodrag nopan ${isSoloed ? "is-solo" : ""}`} onClick={(e) => { e.stopPropagation(); toggleChipSolo(node.id); }}>S</button>
          </div>
        ) : null}
      </div>
      <div className="nk-chip-foot" title={chipFootLine(node.id, peak)} style={{ visibility: bindOpen ? "hidden" : "visible" }}>
        <span>{node.id}</span>
        <span>{chipFootPeak(peak)}</span>
      </div>
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
      {bindRailVisible(bindOpen, binds) ? (
        <BindRail
          nodeId={node.id}
          face={bindFace()}
          width={node.w}
          rows={binds.map((key) => ({ key, formula: node.args[key] ?? "" }))}
        />
      ) : null}
      {ports.map((p) => {
        const loc = portLocal(node, p);
        return (
          <button
            key={p.id}
            type="button"
            className={`nk-port ${p.east ? "nk-port-east" : "nk-port-west"} nk-port-${p.kind}`}
            data-port-id={p.id}
            data-tip={portBracket(p.jackId)}
            style={{
              top: loc.y,
              width: BOARD_HIT,
              height: BOARD_HIT,
            }}
          >
            <i className="nk-port-contact" />
            <span className="nk-port-cap">{portBracket(p.jackId)}</span>
          </button>
        );
      })}
      {ports.map((p) => {
        if (! p.east || p.kind !== "audio") {
          return null;
        }
        const loc = portLocal(node, p);
        const j = p.jackId.toLowerCase();
        const jackPeak = j === "mid" || j === "left" || j === "l"
          ? peakL
          : j === "side" || j === "right" || j === "r"
            ? peakR
            : peak;
        const hot = overloadLabel(jackPeak);
        if (! hot) {
          return null;
        }
        return (
          <span
            key={`warn-${p.id}`}
            className="nk-overload"
            aria-hidden
            style={{ top: p.count > 1 ? loc.y : "50%" }}
          >
            <ClipWarnSvg />
            <em>{hot}</em>
          </span>
        );
      })}
      {bindOpen ? (
        <BindDropPad nodeId={node.id} keys={binds} args={node.args} w={node.w} h={node.h} local={bindLocal} />
      ) : null}
    </div>
  );
}

function ClipWarnSvg() {
  const mark = clipWarnMark();
  return (
    <svg viewBox={mark.viewBox} aria-hidden>
      <polygon
        points={mark.triangle}
        fill={mark.fill}
        stroke="currentColor"
        strokeWidth="2"
        strokeLinejoin="miter"
      />
      <path d={mark.stem} fill={mark.fill} stroke="currentColor" strokeWidth="2" strokeLinecap="square" />
      <circle cx={mark.dot.cx} cy={mark.dot.cy} r={mark.dot.r} fill="currentColor" />
    </svg>
  );
}

