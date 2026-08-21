import { Handle, Position, useReactFlow, useStore, useUpdateNodeInternals, type NodeProps } from "@xyflow/react";
import { useLayoutEffect, useMemo, useState } from "react";
import type { AstJack } from "../bridge/ast";
import { useChipViewStore } from "../store/expandStore";
import { useHostStore } from "../store/hostStore";
import { useBindStore, useTelemetryStore } from "../store/telemetryStore";
import { peakToDb } from "../bridge/telemetry";
import { barcodeBits, CHIP_FRAME_DASH, chipExpandOffset, DETAIL_HIT, framePoints, segmentFill } from "../theme/chromeSpec";
import { formatMapped } from "../theme/tokens";
import { renameCircuitBlock, setCircuitArg, addCircuitBlock, insertCircuitBlockBetween, ADDABLE_BLOCKS } from "./addBlock";
import { isFlowBlockId } from "./muteSolo";
import { toggleChipMute, toggleChipSolo } from "./muteSoloApply";
import { bindEndId } from "./bindLinks";
import { handleId } from "./handles";
import { commitBind } from "./bindModel";
import { primaryJackId } from "./connectModel";
import { bindFace, bindJackCaption, bindJackXs, chipBox, jackCaption, jackTopPx, LABEL_COL } from "./chipLayout";
import { collapsedFace, paintedBindKeys } from "./chipSpec";
import { detailArgs, isCustomNode, nextCustomInput } from "./detailSchema";
import { isLfoNode, isEnvNode, type ChipData } from "./flowFromAst";
import { lfoPeriodMs, parseLfoShape, resolveLfoHz } from "./lfoLamp";
import { liveArg } from "./liveArg";
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
        title={jack.label || jack.id}
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
    >
      <button
        type="button"
        className="nk-sock-hit nodrag"
        title={`bind ${argKey}`}
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
        <span className="nk-sock-meta">
          <span className="nk-sock-key">{argKey}</span>
          <span className="nk-sock-letter">{bound || "·"}</span>
        </span>
        <span className="nk-seg" aria-hidden>
          {Array.from({ length: 10 }, (_, i) => (
            <i key={i} className={i < segmentFill(live) ? "on" : ""} />
          ))}
        </span>
      </button>
      <input
        className="nk-sock-live nodrag font-mono"
        defaultValue={formula}
        title={live}
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
  const dragging = useBindStore((s) => s.letter);
  return (
    <div className="nk-bind-rail" data-bind-face={face} style={face === "top" ? { top: 0 } : { bottom: 0 }}>
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
            title={`plug ${row.key}`}
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
            <span className="nk-bind-jack-cap">{bindJackCaption(row.key)}</span>
          </button>
        );
      })}
    </div>
  );
}

export function ChipNode({ data, id }: NodeProps) {
  const d = data as ChipData;
  const focus = String((data as { focus?: string }).focus ?? "off");
  const detail = useChipViewStore((s) => Boolean(s.detail[id]));
  const toggle = useChipViewStore((s) => s.toggle);
  const setDetail = useChipViewStore((s) => s.setDetail);

  const isMuted = useChipViewStore((s) => s.isMuted(id));
  const isSoloed = useChipViewStore((s) => s.isSoloed(id));
  const isAudible = useChipViewStore((s) => s.isAudible(id));
  const dragging = useBindStore((s) => s.letter);
  const knobs = useHostStore((s) => s.knobs);
  const { getEdges } = useReactFlow();
  const bpm = useHostStore((s) => s.bpm);
  const irLoaded = useHostStore((s) => s.irSlots.some((slot) => slot.slot === id && slot.loaded));
  const peak = useTelemetryStore((s) => (d.type === "in" ? s.inPeak : s.outPeak));
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
  const box = chipBox(isCustomNode(d.type, id) ? "custom" : d.type, d.jacks, detail, d.args);
  const liveRows = extra.map((row) => ({ key: row.key, ...liveArg(row.value, knobs), bind: binds.includes(row.key) }));
  const custom = isCustomNode(d.type, id);
  const [rename, setRename] = useState(false);
  const [showAddPicker, setShowAddPicker] = useState(false);
  const updateInternals = useUpdateNodeInternals();
  const { setNodes } = useReactFlow();
  const absY = useStore((s) => s.nodeLookup.get(id)?.internals.positionAbsolute.y ?? 0);
  const paneH = useStore((s) => s.height);
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
      data-audible={isAudible ? "on" : "off"}
      style={{
        width: box.w,
        height: box.h,
        ["--nk-label-col" as string]: `${LABEL_COL}px`,
        opacity: isAudible ? 1 : 0.4,
      }}
      onPointerEnter={() => {
        if (dragging) {
          setDetail(id, true);
        }
      }}
    >
      <div className="nk-chip-fill" />
      <button
        type="button"
        className="nk-chip-expand nodrag nopan"
        data-chip-expand={id}
        aria-expanded={detail}
        title={detail ? "Hide details" : "Show details"}
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
          stroke="var(--nk-accent)"
          strokeWidth="1.15"
          strokeDasharray={CHIP_FRAME_DASH}
          strokeLinejoin="miter"
        />
      </svg>
      <div className="nk-chip-body">
        <div className="flex h-[26px] w-full items-center justify-between gap-2">
          <span className="nk-chip-grip" aria-hidden title="Drag" />
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
              title={custom ? "Click to rename" : undefined}
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
                title={`${shape} ${formatMapped(hz)} Hz`}
                aria-hidden
              />
            ) : null}
            {warn ? <span className="nk-warn-tri" title="hot" /> : null}
            {d.channel ? (
              <span className="text-[9px] text-muted">{d.channel}</span>
            ) : null}
          </span>
        </div>
        <div className="mt-0.5 flex items-end justify-between gap-2">
          <span className="nk-chip-typecode">{face.code}</span>
          {isIrSlotId(id) ? (
            <button
              type="button"
              className="nk-clip nodrag nopan px-1 text-[10px]"
              title="Load cabinet IR"
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

        {canMuteSolo || detail ? (
          <div className="mt-1 flex items-center gap-1">
            {canMuteSolo ? (
              <>
                <button
                  type="button"
                  className={`nodrag nopan min-h-[26px] min-w-[26px] px-1 text-[11px] border ${isMuted ? "bg-warn/20 border-warn text-warn" : "border-accent/40 text-muted hover:text-ink"}`}
                  title="Mute block"
                  onClick={(e) => {
                    e.stopPropagation();
                    toggleChipMute(id);
                  }}
                >
                  M
                </button>
                <button
                  type="button"
                  className={`nodrag nopan min-h-[26px] min-w-[26px] px-1 text-[11px] border ${isSoloed ? "bg-accent/20 border-accent text-accent" : "border-accent/40 text-muted hover:text-ink"}`}
                  title="Solo block"
                  onClick={(e) => {
                    e.stopPropagation();
                    toggleChipSolo(id);
                  }}
                >
                  S
                </button>
              </>
            ) : null}
            {detail ? (
              <button
                type="button"
                className="nodrag nopan ml-auto min-h-[26px] min-w-[26px] border border-accent/40 px-1 text-[14px] text-accent hover:text-ink"
                title="Add block after this one"
                onClick={(e) => {
                  e.stopPropagation();
                  setShowAddPicker((v) => ! v);
                }}
              >
                +
              </button>
            ) : null}
          </div>
        ) : null}

        {detail ? (
          <div className="nk-sock-list">
            {liveRows.map((row) => (
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
        ) : null}

        {detail ? (
          <div className="mt-1 truncate text-[10px] text-muted">
            {lfo ? `hz ${formatMapped(hz)}` : env ? `env ${envLevel.toFixed(2)}` : `peak ${peakToDb(peak).toFixed(1)} dB`}
          </div>
        ) : null}
      </div>

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
        <JackPort key={`in-${j.id}`} jack={j} output={false} top={jackTopPx(i, Math.max(ins.length, 1), box.h)} />
      ))}
      {outs.map((j, i) => (
        <JackPort key={`out-${j.id}`} jack={j} output top={jackTopPx(i, Math.max(outs.length, 1), box.h)} />
      ))}
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
          top={jackTopPx(0, 1, box.h)}
        />
      ) : null}
      {outs.length === 0 && ! bottomJacks.some((j) => j.output) && ! topJacks.some((j) => j.output) ? (
        <JackPort
          jack={{ id: primaryJackId(d.jacks, true), label: "out", output: true, kind: "audio" }}
          output
          top={jackTopPx(0, 1, box.h)}
        />
      ) : null}
      
      {showAddPicker && detail ? (
        <div
          className="nodrag nopan absolute z-10 max-h-48 overflow-y-auto border border-accent bg-black p-2 text-[11px]"
          style={{
            top: absY + box.h + 200 > paneH ? undefined : box.h,
            bottom: absY + box.h + 200 > paneH ? box.h : undefined,
            left: 0,
            minWidth: Math.max(box.w, 168),
          }}
          onClick={(e) => e.stopPropagation()}
        >
          <div className="mb-1 text-muted">Add block after {id}:</div>
          <div className="grid grid-cols-2 gap-1">
            {ADDABLE_BLOCKS.filter((b) => b.category !== "Mod").slice(0, 12).map((block) => (
              <button
                key={block.type + block.label}
                type="button"
                className="border border-accent/40 px-2 py-1 text-left text-ink hover:bg-accent/10"
                onClick={() => {
                  // Find the first output edge from this node
                  const edges = getEdges();
                  const outputEdge = edges.find((e) => e.source === id);
                  
                  if (outputEdge) {
                    // Insert between this node and the target
                    insertCircuitBlockBetween(block.type, id, outputEdge.target, block.args);
                  } else {
                    // No output edge, just add normally
                    addCircuitBlock(block.type, block.args);
                  }
                  setShowAddPicker(false);
                }}
              >
                {block.label}
              </button>
            ))}
          </div>
          <button
            type="button"
            className="mt-2 w-full border border-accent/40 py-1 text-muted hover:text-ink"
            onClick={() => setShowAddPicker(false)}
          >
            Cancel
          </button>
        </div>
      ) : null}
    </div>
  );
}

export function IoNode({ data, id }: NodeProps) {
  const d = data as ChipData;
  const focus = String((data as { focus?: string }).focus ?? "off");
  const isIn = d.type === "in" || d.type === "sidechain";
  const detail = useChipViewStore((s) => Boolean(s.detail[id]));
  const knobs = useHostStore((s) => s.knobs);
  const extra = useMemo(() => detailArgs(d.type, d.args), [d.args, d.type]);
  const box = chipBox(d.type, d.jacks, detail, d.args);
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
      style={{
        width: box.w,
        height: box.h,
        paddingLeft: isIn ? 12 : LABEL_COL,
        paddingRight: isIn ? LABEL_COL : 12,
        ["--nk-label-col" as string]: `${LABEL_COL}px`,
      }}
    >
      <div className="nk-chip-fill" />
      <svg className="nk-chip-frame" width={box.w} height={box.h} viewBox={`0 0 ${box.w} ${box.h}`} aria-hidden>
        <polygon
          points={framePoints(box.w, box.h)}
          fill="none"
          stroke={isIn ? "var(--nk-cyan)" : "var(--nk-warn)"}
          strokeWidth="1.15"
          strokeDasharray={CHIP_FRAME_DASH}
          strokeLinejoin="miter"
        />
      </svg>
      <div className="relative z-[2] flex w-full items-center justify-center gap-1">
        <span className="nk-chip-title truncate">{d.type === "sidechain" ? "Sidechain" : d.label}</span>
      </div>
      {detail ? (
        <div className="nk-sock-list relative z-[2] w-full px-1">
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
      ) : null}
      {d.jacks.map((j, i) => (
        <JackPort
          key={j.id}
          jack={j}
          output={j.output}
          top={jackTopPx(i, Math.max(d.jacks.length, 1), box.h)}
        />
      ))}
    </div>
  );
}
