import type { CSSProperties } from "react";
import { themeOf } from "../theme/theme";
import { useChipViewStore } from "../store/expandStore";
import { useHostStore } from "../store/hostStore";
import { BindDropPad, BindRail } from "./BindRail";
import { chipExpandAction } from "./boardEdit";
import {
  bindRailVisible,
  chipKindMark,
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
import { lfoLampClass, lfoPeriodMs, parseLfoShape, resolveLfoHz } from "./lfoLamp";
import { ChipShell } from "./nodeFace/ChipShell";
import { NodeFace } from "./nodeFace/NodeFace";
import { chipDepth } from "./nodeFace/chipChrome";
import { faceKindForType } from "./nodeFace/faceKind";

const NO_KNOBS: Array<{ id: string; value: number; min: number; max: number; isNote?: boolean }> = [];

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
  const knobs = useHostStore((s) => (node.type === "osc" ? s.knobs : NO_KNOBS));
  const bpm = useHostStore((s) => s.bpm);
  const theme = useHostStore((s) => s.theme);
  const inspectOpen = useHostStore((s) => s.inspectId === node.id);
  const chrome = closedChipChrome(node.type, node.id);
  const face = collapsedFace(node.type, node.args);
  const title = node.role === "io" ? node.label : face.title;
  const canMs = ! isFlowBlockId(node.id) && node.role === "chip";
  const ioInset = node.role === "io" ? ioBodyInset() : null;
  const showExpand = chipExpandAction(node.role, node.type) === "inspect";
  const binds = node.role === "chip" ? paintedBindKeys(node.type, node.args) : [];
  const bindOpen = bindOver && node.role === "chip" && binds.length > 0;
  const lfoShape = parseLfoShape(node.args.shape ?? "sine");
  const lfoHz = node.type === "osc" ? resolveLfoHz(node.args, knobs, bpm) : 0;
  const frameCut = themeOf(theme).frameCut;
  const kind = faceKindForType(node.type);
  const depth = chipDepth({ selected, focus, bindOpen });

  let headerStart = null;
  if (node.role === "chip") {
    if (chrome.lamp === "clip") {
      headerStart = <ChipClipLamp nodeId={node.id} />;
    } else if (chrome.lamp === "env") {
      headerStart = <ChipEnvLamp nodeId={node.id} />;
    } else if (chrome.lamp === "lfo") {
      headerStart = (
        <span
          className={`nk-lfo-lamp ${lfoLampClass(lfoShape)}`}
          style={{ "--nk-lfo-ms": `${lfoPeriodMs(lfoHz)}ms` } as CSSProperties}
          aria-hidden
        />
      );
    }
  }

  return (
    <ChipShell
      nodeId={node.id}
      role={node.role}
      selected={selected}
      kind={kind}
      depth={depth}
      audible={isAudible}
      title={title}
      typecode={face.code}
      frameCut={frameCut}
      inAccent={node.type === "in"}
      bindOpen={bindOpen}
      showExpand={showExpand}
      inspectOpen={inspectOpen}
      onInspect={onInspect}
      headerStart={headerStart}
      bodyStart={canMs ? (
        <div className="nk-chip-ms flex items-center gap-1" style={{ pointerEvents: "auto" }}>
          <button type="button" className={`nk-ms nodrag nopan ${isMuted ? "is-on" : ""}`} onClick={(e) => { e.stopPropagation(); toggleChipMute(node.id); }}>M</button>
          <button type="button" className={`nk-ms nodrag nopan ${isSoloed ? "is-solo" : ""}`} onClick={(e) => { e.stopPropagation(); toggleChipSolo(node.id); }}>S</button>
        </div>
      ) : null}
      data-selected={selected ? "on" : "off"}
      data-focus={bindOpen ? "sharp" : (focus ?? "off")}
      data-kind={chipKindMark(node.type, node.id, node.role)}
      data-io={node.role === "io" ? node.type : undefined}
      style={{
        position: "absolute",
        left: node.x,
        top: node.y,
        width: node.w,
        height: node.h,
        ...chipChromeVars(node.role),
        ...(ioInset
          ? {
            "--nk-label-col": `${ioInset.left}px`,
            "--nk-south-gap": `${ioInset.bottom}px`,
          } as CSSProperties
          : {}),
      }}
    >
      <NodeFace kind={kind} type={node.type} args={node.args} id={node.id} />
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
      <ChipOverloadMarks node={node} ports={ports} />
      {bindOpen ? (
        <BindDropPad nodeId={node.id} keys={binds} args={node.args} w={node.w} h={node.h} local={bindLocal} />
      ) : null}
    </ChipShell>
  );
}

function ChipClipLamp({ nodeId }: { nodeId: string }) {
  const peak = useHostStore((s) => s.clips[nodeId] ?? 0);
  return <span className={`nk-clip-lamp${peak >= 1 ? " is-clip" : ""}`} aria-hidden />;
}

function ChipEnvLamp({ nodeId }: { nodeId: string }) {
  const envLevel = useHostStore((s) => {
    const v = s.mods[nodeId];
    return Number.isFinite(v) ? Math.max(0, Math.min(1, v as number)) : 0;
  });
  return (
    <span
      className="nk-env-lamp"
      aria-hidden
      style={{ "--nk-env": String(envLevel) } as CSSProperties}
    />
  );
}

function ChipOverloadMarks({ node, ports }: { node: BoardNode; ports: BoardPort[] }) {
  const peak = useHostStore((s) => s.clips[node.id] ?? 0);
  const peakL = useHostStore((s) => s.clipsL[node.id] ?? peak);
  const peakR = useHostStore((s) => s.clipsR[node.id] ?? peak);
  return (
    <>
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
            style={{ top: loc.y }}
          >
            <ClipWarnSvg />
            <em>{hot}</em>
          </span>
        );
      })}
    </>
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
