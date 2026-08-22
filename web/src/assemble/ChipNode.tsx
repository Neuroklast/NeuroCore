import { Handle, Position, useReactFlow, useStore, useUpdateNodeInternals, type NodeProps } from "@xyflow/react";
import { useLayoutEffect, useMemo, useState, type CSSProperties } from "react";
import type { AstJack } from "../bridge/ast";
import { useChipViewStore } from "../store/expandStore";
import { useHostStore } from "../store/hostStore";
import { useBindStore } from "../store/telemetryStore";
import { peakToDb } from "../bridge/telemetry";
import { barcodeBits, CHIP_CLIP, chipExpandOffset, DETAIL_HIT, frameCorners, framePoints, greebleCode, satLampOn } from "../theme/chromeSpec";
import { formatMapped } from "../theme/tokens";
import { renameCircuitBlock, setCircuitArg } from "./addBlock";
import { isFlowBlockId } from "./muteSolo";
import { toggleChipMute, toggleChipSolo } from "./muteSoloApply";
import { bindEndId } from "./bindLinks";
import { handleId } from "./handles";
import { commitBind } from "./bindModel";
import { primaryJackId } from "./connectModel";
import { bindFace, bindJackCaption, bindCaptionMaxPx, bindJackXs, chipBox, chipChromeVars, chipOverlayStackPx, jackCaption, jackTopPx, LABEL_COL, TITLE_H } from "./chipLayout";
import { canonicalIoJacks, ioFaceWidgets } from "./ioPaint";
import { CHIP_PAD_Y } from "./chipMetrics";
import { collapsedFace, paintedBindKeys } from "./chipSpec";
import { detailArgs, isCustomNode, nextCustomInput } from "./detailSchema";
import { isLfoNode, isEnvNode, type ChipData } from "./flowFromAst";
import { lfoPeriodMs, parseLfoShape, resolveLfoHz } from "./lfoLamp";
import { liveArg } from "./liveArg";
import { cableSourcePeak } from "./cableMotion";
import { cableFace } from "./validateLink";
import { isIrSlotId } from "../presets/irSlots";
import { openImpulse } from "../overlays/ImpulsePanel";

function JackPort({
  jack,
  output,
  top,
  left,
  face = "side",
  showLabel = true,
}: {
  jack: AstJack;
  output: boolean;
  top?: number;
  left?: number;
  face?: "side" | "bottom" | "top";
  showLabel?: boolean;
}) {
  const position = face === "bottom"
    ? Position.Bottom
    : face === "top"
      ? Position.Top
      : (output ? Position.Right : Position.Left);
  const style = face === "side"
    ? { top: `${top ?? 0}px` }
    : { left: `${left ?? 0}px` };
  return (
    <>
      {face === "side" ? (
        <span className={`nk-jack-shell ${output ? "nk-jack-shell-out" : "nk-jack-shell-in"}`} style={{ top: `${top ?? 0}px` }} />
      ) : null}
      <Handle
        type={output ? "source" : "target"}
        position={position}
        id={handleId(jack.id, output)}
        className={`nk-plug ${output ? "nk-plug-out" : "nk-plug-in"} ${jack.kind === "mod" ? "nk-plug-mod" : ""} ${face !== "side" ? "nk-plug-face" : ""}`}
        style={style}
        isConnectableStart={output}
        isConnectableEnd={! output}
        data-tip={jack.label || jack.id}
      />
      {showLabel && face === "side" ? (
        <span
          className={`nk-plug-label ${output ? "nk-plug-label-out" : "nk-plug-label-in"} ${jack.kind === "mod" ? "nk-plug-label-mod" : ""}`}
          style={{ top: `${top ?? 0}px` }}
        >
          {jackCaption(jack)}
        </span>
      ) : null}
    </>
  );
}

function ParamSocket({
  nodeId,
  argKey,
  formula,
  live,
}: {
  nodeId: string;
  argKey: string;
  formula: string;
  live: string;
}) {
  const dragging = useBindStore((s) => s.letter);
  const [pick, setPick] = useState(false);
  const bound = /^[a-f]$/i.test(formula.trim()) ? formula.trim().toLowerCase() : "";
  return (
    <div
      className={`nk-sock nodrag ${dragging ? "nk-sock-hot" : ""} ${bound ? "nk-sock-on" : ""}`}
      data-bind-node={nodeId}
      data-bind-key={argKey}
      data-chip-keep-open=""
    >
      <button
        type="button"
        className="nk-sock-hit nodrag"
        data-tip={`bind ${argKey}`}
        onClick={(e) => {
          e.stopPropagation();
          if (dragging) {
            commitBind(nodeId, argKey, dragging);
            useBindStore.getState().end();
            return;
          }
          setPick((v) => ! v);
        }}
      >
        <span className="nk-sock-key">{argKey}</span>
      </button>
      <div className="nk-sock-row">
      <input
        className="nk-sock-live nodrag font-mono"
        defaultValue={formula}
        data-tip={live}
        onClick={(e) => e.stopPropagation()}
        onPointerDown={(e) => e.stopPropagation()}
        onBlur={(e) => {
          const next = e.target.value.trim();
          if (next && next !== formula) {
            setCircuitArg(nodeId, argKey, next);
          }
        }}
        onKeyDown={(e) => {
          if (e.key === "Enter") {
            (e.target as HTMLInputElement).blur();
          }
        }}
      />
      {bound ? <span className="nk-bind-badge">{bound}</span> : null}
      </div>
      {pick ? (
        <div className="nk-sock-pick">
          {["a", "b", "c", "d", "e", "f"].map((L) => (
            <button
              key={L}
              type="button"
              className={`nk-sock-opt ${bound === L ? "on" : ""}`}
              onClick={(e) => {
                e.stopPropagation();
                commitBind(nodeId, argKey, L);
                setPick(false);
              }}
            >
              {L}
            </button>
          ))}
        </div>
      ) : null}
    </div>
  );
}

function BindRail({
  nodeId,
  face,
  width,
  rows,
}: {
  nodeId: string;
  face: "top" | "bottom";
  width: number;
  rows: Array<{ key: string; formula: string; live: string }>;
}) {
  const xs = bindJackXs(rows.length, width);
  const capW = bindCaptionMaxPx(rows.length, width);
  const dragging = useBindStore((s) => s.letter);
  return (
    <div className="nk-bind-rail" data-bind-face={face} data-chip-keep-open="" style={face === "top" ? { top: 0 } : { bottom: 0 }}>
      {rows.map((row, i) => {
        const bound = /^[a-f]$/i.test(row.formula.trim()) ? row.formula.trim().toLowerCase() : "";
        const letters = bound ? [bound] : [];
        if (letters.length === 0) {
          for (const L of row.formula.match(/\b[a-f]\b/gi) ?? []) {
            const n = L.toLowerCase();
            if (! letters.includes(n)) {
              letters.push(n);
            }
          }
        }
        return (
          <button
            key={row.key}
            type="button"
            className={`nk-bind-jack nodrag ${bound || letters.length ? "on" : ""} ${dragging ? "hot" : ""}`}
            style={{ left: xs[i] }}
            data-bind-node={nodeId}
            data-bind-key={row.key}
            data-tip={`plug ${row.key}`}
            onClick={(e) => {
              e.stopPropagation();
              if (dragging) {
                commitBind(nodeId, row.key, dragging);
                useBindStore.getState().end();
              }
            }}
          >
            {letters.map((L) => (
              <span key={L} data-bind-end={bindEndId(L, nodeId)} className="nk-bind-end" />
            ))}
            <span className="nk-bind-jack-cap" style={{ maxWidth: capW }} data-tip={bindJackCaption(row.key)}>
              {bindJackCaption(row.key)}
            </span>
          </button>
        );
      })}
    </div>
  );
}

export function ChipNode({ data, id, selected }: NodeProps) {
  const d = data as ChipData;
  const focus = String((data as { focus?: string }).focus ?? "off");
  const detail = useChipViewStore((s) => Boolean(s.detail[id]));
  const toggle = useChipViewStore((s) => s.toggle);

  const isMuted = useChipViewStore((s) => s.isMuted(id));
  const isSoloed = useChipViewStore((s) => s.isSoloed(id));
  const isAudible = useChipViewStore((s) => s.isAudible(id));
  const knobs = useHostStore((s) => s.knobs);
  const bpm = useHostStore((s) => s.bpm);
  const irLoaded = useHostStore((s) => s.irSlots.some((slot) => slot.slot === id && slot.loaded));
  const peak = useHostStore((s) => cableSourcePeak(id, d.type, s.clips));
  const sideJacks = d.jacks.filter((j) => j.kind !== "knob" && cableFace(j.kind) === "side");
  const bottomJacks = d.jacks.filter((j) => j.kind !== "knob" && cableFace(j.kind) === "bottom");
  const topJacks = d.jacks.filter((j) => j.kind !== "knob" && cableFace(j.kind) === "top");
  const ins = sideJacks.filter((j) => ! j.output);
  const outs = sideJacks.filter((j) => j.output);
  const typeId = isCustomNode(d.type, id) ? "custom" : d.type;
  const binds = paintedBindKeys(typeId, d.args);
  const lfo = isLfoNode({ type: d.type, id });
  const env = isEnvNode({ type: d.type, id });
  const envLevel = useHostStore((s) => s.mods[id] ?? 0);
  const hz = lfo ? resolveLfoHz(d.args, knobs, bpm) : 0;
  const extra = useMemo(() => detailArgs(isCustomNode(d.type, id) ? "custom" : d.type, d.args), [d.args, d.type, id]);
  const box = chipBox(isCustomNode(d.type, id) ? "custom" : d.type, d.jacks, false, d.args);
  const liveRows = extra.map((row) => ({ key: row.key, ...liveArg(row.value, knobs), bind: binds.includes(row.key) }));
  const overlayRows = liveRows;
  const custom = isCustomNode(d.type, id);
  const overlayH = chipOverlayStackPx(overlayRows.length + (custom ? 1 : 0) + 1);
  const [rename, setRename] = useState(false);
  const updateInternals = useUpdateNodeInternals();
  const { setNodes } = useReactFlow();
  const absY = useStore((s) => s.nodeLookup.get(id)?.internals.positionAbsolute.y ?? 0);
  const shape = lfo ? parseLfoShape(d.args.shape ?? d.args.wave ?? "sine") : "sine";
  const lampMs = lfo ? lfoPeriodMs(hz) : 0;
  const expandAt = chipExpandOffset();
  
  const canMuteSolo = ! isFlowBlockId(id);

  useLayoutEffect(() => {
    setNodes((ns) => {
      const cur = ns.find((n) => n.id === id);
      if (cur && cur.height === box.h && cur.width === box.w) {
        return ns;
      }
      return ns.map((n) => (
        n.id === id
          ? { ...n, height: box.h, width: box.w, style: { ...n.style, width: box.w, height: box.h } }
          : n
      ));
    });
    updateInternals(id);
  }, [box.h, box.w, id, setNodes, updateInternals]);

  const face = collapsedFace(isCustomNode(d.type, id) ? "custom" : d.type, d.args);
  const title = custom ? id : face.title;
  const bits = barcodeBits(id);
  const warn = /stage|drive|clip|fold|crush/i.test(d.type + title);
  const bottomXs = bindJackXs(bottomJacks.length, box.w);
  const topXs = bindJackXs(topJacks.length, box.w);
  return (
    <div
      className="nk-chip relative text-[12px]"
      data-node-id={id}
      data-bind-node={id}
      data-focus={focus}
      data-selected={selected ? "on" : "off"}
      data-audible={isAudible ? "on" : "off"}
      data-detail={detail ? "on" : "off"}
      style={{
        width: box.w,
        height: box.h,
        ...(chipChromeVars() as CSSProperties),
        opacity: isAudible ? 1 : 0.4,
      }}
    >
      <div className="nk-chip-fill" />
      <button
        type="button"
        className="nk-chip-expand nodrag nopan"
        data-chip-expand={id}
        data-chip-keep-open=""
        aria-expanded={detail}
        data-tip={detail ? "Hide details" : "Show details"}
        style={{
          position: "absolute",
          top: expandAt.top,
          right: expandAt.right,
          zIndex: 6,
          minWidth: DETAIL_HIT,
          minHeight: DETAIL_HIT,
          width: expandAt.size,
          height: expandAt.size,
        }}
        onClick={(e) => {
          e.stopPropagation();
          toggle(id);
        }}
      >
        {detail ? "▴" : "▾"}
      </button>
      <svg className="nk-chip-frame" width={box.w} height={box.h} viewBox={`0 0 ${box.w} ${box.h}`} aria-hidden>
        <polygon
          points={framePoints(box.w, box.h)}
          fill="none"
          stroke={selected ? "var(--nk-accent)" : "var(--nk-ink-muted)"}
          strokeWidth="1.4"
          strokeLinejoin="miter"
        />
        {frameCorners(box.w, box.h).map((d, i) => (
          <path
            key={i}
            d={d}
            fill="none"
            stroke={selected ? "var(--nk-accent)" : "var(--nk-ink-soft)"}
            strokeWidth="2.7"
            strokeLinejoin="miter"
            strokeLinecap="square"
          />
        ))}
      </svg>
      <div className="nk-chip-body">
        <div className="flex w-full items-center justify-between gap-2" style={{ height: TITLE_H }}>
          <span className="nk-chip-grip" aria-hidden data-tip="Drag" />
          {custom && rename ? (
            <input
              className="nk-chip-title-edit nodrag nopan min-w-0 flex-1 truncate font-mono text-ink"
              defaultValue={id}
              autoFocus
              onClick={(e) => e.stopPropagation()}
              onPointerDown={(e) => e.stopPropagation()}
              onBlur={(e) => {
                const next = e.target.value.trim();
                setRename(false);
                if (next && next !== id) {
                  renameCircuitBlock(id, next);
                }
              }}
              onKeyDown={(e) => {
                if (e.key === "Enter") {
                  (e.target as HTMLInputElement).blur();
                }
                if (e.key === "Escape") {
                  setRename(false);
                }
              }}
            />
          ) : (
            <span
              className={`nk-chip-title min-w-0 flex-1 truncate text-ink ${custom ? "nodrag" : ""}`}
              data-tip={custom ? "Click to rename" : undefined}
              onClick={(e) => {
                if (! custom) {
                  return;
                }
                e.stopPropagation();
                setRename(true);
              }}
            >
              {title}
            </span>
          )}
          <span className="flex shrink-0 items-center gap-1">
            {lfo ? (
              <span
                className={`nk-lfo-lamp nk-lfo-${shape}`}
                style={{ ["--nk-lfo-ms" as string]: `${lampMs}ms` }}
                data-tip={`${shape} ${formatMapped(hz)} Hz`}
                aria-hidden
              />
            ) : (
              <span
                className={`nk-clip-lamp${peak >= 1 ? " is-clip" : ""}`}
                data-tip={peak >= 1 ? "clip" : "clip lamp"}
                aria-hidden
              />
            )}
            {warn ? <span className="nk-warn-tri" data-tip="hot" /> : null}
            {d.channel ? (
              <span className="text-[9px] text-muted">{d.channel}</span>
            ) : null}
          </span>
        </div>
        <div className="mt-0.5 flex items-end justify-between gap-2">
          <span className="nk-chip-typecode">{face.code}</span>
          <span className="nk-greeble-id">{greebleCode(id)}</span>
          {isIrSlotId(id) ? (
            <button
              type="button"
              className="nk-clip nodrag nopan px-1 text-[10px]"
              data-tip="Load cabinet IR"
              onClick={(e) => {
                e.stopPropagation();
                openImpulse(id);
              }}
            >
              {irLoaded ? "CAB" : "LOAD"}
            </button>
          ) : (
            <span className="nk-barcode" aria-hidden>
              {bits.map((on, i) => (
                <i key={i} style={{ height: on ? 8 : 3 }} />
              ))}
            </span>
          )}
        </div>

        {canMuteSolo ? (
          <>
            <div className="nk-chip-rule" />
            <div className="flex items-center gap-1">
              <button
                type="button"
                className={`nk-ms nodrag nopan ${isMuted ? "is-on" : ""}`}
                data-tip="Mute block"
                onClick={(e) => {
                  e.stopPropagation();
                  toggleChipMute(id);
                }}
              >
                M
              </button>
              <button
                type="button"
                className={`nk-ms nodrag nopan ${isSoloed ? "is-solo" : ""}`}
                data-tip="Solo block"
                onClick={(e) => {
                  e.stopPropagation();
                  toggleChipSolo(id);
                }}
              >
                S
              </button>
            </div>
          </>
        ) : null}

      </div>

      {detail ? (
        <div
          className="nk-chip-overlay nodrag nopan nowheel"
          data-chip-keep-open=""
          style={{ height: overlayH, clipPath: CHIP_CLIP }}
        >
          <div className="nk-chip-overlay-fill" />
          <svg className="nk-chip-frame" width={box.w} height={overlayH} viewBox={`0 0 ${box.w} ${overlayH}`} aria-hidden>
            <polygon
              points={framePoints(box.w, overlayH)}
              fill="none"
              stroke={selected ? "var(--nk-accent)" : "var(--nk-ink-muted)"}
              strokeWidth="1.4"
              strokeLinejoin="miter"
            />
            {frameCorners(box.w, overlayH).map((d, i) => (
              <path
                key={i}
                d={d}
                fill="none"
                stroke={selected ? "var(--nk-accent)" : "var(--nk-ink-soft)"}
                strokeWidth="2.7"
                strokeLinejoin="miter"
                strokeLinecap="square"
              />
            ))}
          </svg>
          <div className="nk-chip-overlay-body">
            <div className="nk-sock-list">
              {overlayRows.map((row) => (
                <ParamSocket
                  key={row.key}
                  nodeId={id}
                  argKey={row.key}
                  formula={row.formula}
                  live={row.live}
                />
              ))}
              {custom ? (
                <button
                  type="button"
                  className="nk-chip-add-in nodrag nopan"
                  onClick={(e) => {
                    e.stopPropagation();
                    const key = nextCustomInput(d.args);
                    setCircuitArg(id, key, "0");
                  }}
                >
                  + input
                </button>
              ) : null}
            </div>
            <div className="nk-chip-overlay-meta truncate">
              {lfo ? `hz ${formatMapped(hz)}` : env ? `env ${envLevel.toFixed(2)}` : `peak ${peakToDb(peak).toFixed(1)} dB`}
            </div>
          </div>
        </div>
      ) : null}

      {binds.length > 0 ? (
        <BindRail
          nodeId={id}
          face={bindFace(absY, box.h)}
          width={box.w}
          rows={binds.map((key) => {
            const row = liveRows.find((r) => r.key === key);
            return row
              ? { key: row.key, formula: row.formula, live: row.live }
              : { key, ...liveArg(d.args[key] ?? "", knobs) };
          })}
        />
      ) : null}

      {ins.map((j, i) => (
        <JackPort key={`in-${j.id}`} jack={j} output={false} top={jackTopPx(i, Math.max(ins.length, 1), box.h, typeId)} />
      ))}
      {outs.map((j, i) => {
        const top = jackTopPx(i, Math.max(outs.length, 1), box.h, typeId);
        return (
          <span key={`out-${j.id}`}>
            <JackPort jack={j} output top={top} />
            <i className={`nk-sat${satLampOn(peakToDb(peak)) ? " is-hot" : ""}`} style={{ top: `${top}px` }} />
          </span>
        );
      })}
      {bottomJacks.map((j, i) => (
        <JackPort
          key={`bot-${j.id}`}
          jack={j}
          output={j.output}
          face="bottom"
          left={bottomXs[i] ?? box.w * 0.5}
          showLabel={false}
        />
      ))}
      {topJacks.map((j, i) => (
        <JackPort
          key={`top-${j.id}`}
          jack={j}
          output={j.output}
          face="top"
          left={topXs[i] ?? box.w * 0.5}
          showLabel={false}
        />
      ))}
      {ins.length === 0 && ! lfo && topJacks.every((j) => j.output) && bottomJacks.every((j) => j.output) ? (
        <JackPort
          jack={{ id: primaryJackId(d.jacks, false), label: "in", output: false, kind: "audio" }}
          output={false}
          top={jackTopPx(0, 1, box.h, typeId)}
        />
      ) : null}
      {outs.length === 0 && ! bottomJacks.some((j) => j.output) && ! topJacks.some((j) => j.output) ? (
        <JackPort
          jack={{ id: primaryJackId(d.jacks, true), label: "out", output: true, kind: "audio" }}
          output
          top={jackTopPx(0, 1, box.h, typeId)}
        />
      ) : null}
      
    </div>
  );
}

export function IoNode({ data, id }: NodeProps) {
  const d = data as ChipData;
  const focus = String((data as { focus?: string }).focus ?? "off");
  const isIn = d.type === "in" || d.type === "sidechain";
  const detail = useChipViewStore((s) => Boolean(s.detail[id]));
  const toggle = useChipViewStore((s) => s.toggle);
  const knobs = useHostStore((s) => s.knobs);
  const sidechainOn = useHostStore((s) => s.sidechainOn);
  const peak = useHostStore((s) => s.clips[id] ?? s.clips[isIn ? "IN" : "OUT"] ?? 0);
  const extra = useMemo(() => detailArgs(d.type, d.args), [d.args, d.type]);
  const jacks = d.jacks.length ? d.jacks : canonicalIoJacks(d.type);
  const ins = isIn ? [] : jacks.filter((j) => ! j.output);
  const outs = isIn ? jacks.filter((j) => j.output) : [];
  const box = chipBox(d.type, jacks, false, d.args);
  const expandAt = chipExpandOffset();
  const canExpand = extra.length > 0;
  const updateInternals = useUpdateNodeInternals();
  const { setNodes } = useReactFlow();
  useLayoutEffect(() => {
    setNodes((ns) => ns.map((n) => (
      n.id === id
        ? { ...n, height: box.h, width: box.w, style: { ...n.style, width: box.w, height: box.h } }
        : n
    )));
    updateInternals(id);
  }, [box.h, box.w, id, setNodes, updateInternals]);
  return (
    <div
      className={`nk-chip-io relative flex flex-col items-center justify-center text-[15px] text-ink ${isIn ? "nk-chip-locked" : ""}`}
      data-focus={focus}
      data-detail={detail ? "on" : "off"}
      data-face-widgets={ioFaceWidgets(d.type, false).join(",") || "none"}
      style={{
        width: box.w,
        height: box.h,
        paddingLeft: isIn ? CHIP_PAD_Y : LABEL_COL,
        paddingRight: isIn ? LABEL_COL : CHIP_PAD_Y,
        ...(chipChromeVars() as CSSProperties),
      }}
    >
      <div className="nk-chip-fill" />
      {canExpand ? (
        <button
          type="button"
          className="nk-chip-expand nodrag nopan"
          data-chip-expand={id}
          data-chip-keep-open=""
          aria-expanded={detail}
          data-tip={detail ? "Hide details" : "Show details"}
          style={{
            position: "absolute",
            top: expandAt.top,
            right: expandAt.right,
            zIndex: 6,
            minWidth: DETAIL_HIT,
            minHeight: DETAIL_HIT,
            width: expandAt.size,
            height: expandAt.size,
          }}
          onClick={(e) => {
            e.stopPropagation();
            toggle(id);
          }}
        >
          {detail ? "▴" : "▾"}
        </button>
      ) : null}
      <svg className="nk-chip-frame" width={box.w} height={box.h} viewBox={`0 0 ${box.w} ${box.h}`} aria-hidden>
        <polygon
          points={framePoints(box.w, box.h)}
          fill="none"
          stroke={isIn ? "var(--nk-cyan)" : "var(--nk-ink-muted)"}
          strokeWidth="1.4"
          strokeLinejoin="miter"
        />
        {frameCorners(box.w, box.h).map((path, i) => (
          <path
            key={i}
            d={path}
            fill="none"
            stroke={isIn ? "var(--nk-cyan)" : "var(--nk-ink-soft)"}
            strokeWidth="2.7"
            strokeLinejoin="miter"
            strokeLinecap="square"
          />
        ))}
      </svg>
      <div className="relative z-[2] flex min-w-0 w-full items-center justify-center gap-1 overflow-hidden px-1">
        <span className="nk-chip-title truncate">{d.type === "sidechain" ? "Sidechain" : d.label}</span>
        {isIn ? (
          <span
            className={`nk-lfo-lamp nk-lfo-sine${sidechainOn ? "" : " is-off"}`}
            data-tip="host sidechain"
            aria-hidden
          />
        ) : (
          <span
            className={`nk-clip-lamp${peak >= 1 ? " is-clip" : ""}`}
            data-tip={peak >= 1 ? "clip" : "clip lamp"}
            aria-hidden
          />
        )}
      </div>
      {detail && extra.length > 0 ? (
        <div
          className="nk-chip-overlay nodrag nopan nowheel"
          data-chip-keep-open=""
          style={{ height: chipOverlayStackPx(extra.length), clipPath: CHIP_CLIP }}
        >
          <div className="nk-chip-overlay-fill" />
          <div className="nk-chip-overlay-body">
            <div className="nk-sock-list">
              {extra.map((row) => {
                const live = liveArg(row.value, knobs);
                return (
                  <ParamSocket
                    key={row.key}
                    nodeId={id}
                    argKey={row.key}
                    formula={live.formula}
                    live={live.live}
                  />
                );
              })}
            </div>
          </div>
        </div>
      ) : null}
      {ins.map((j, i) => (
        <JackPort
          key={`in-${j.id}`}
          jack={j}
          output={false}
          top={jackTopPx(i, Math.max(ins.length, 1), box.h, d.type)}
        />
      ))}
      {outs.map((j, i) => (
        <JackPort
          key={`out-${j.id}`}
          jack={j}
          output
          top={jackTopPx(i, Math.max(outs.length, 1), box.h, d.type)}
        />
      ))}
    </div>
  );
}
