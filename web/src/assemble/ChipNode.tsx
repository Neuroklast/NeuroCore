import { Handle, Position, useReactFlow, useStore, useUpdateNodeInternals, type NodeProps } from "@xyflow/react";
import { useLayoutEffect, useMemo, useState } from "react";
import type { AstJack } from "../bridge/ast";
import { useChipViewStore } from "../store/expandStore";
import { useHostStore } from "../store/hostStore";
import { useBindStore, useTelemetryStore } from "../store/telemetryStore";
import { peakToDb } from "../bridge/telemetry";
import { barcodeBits, CHIP_FRAME_DASH, chipExpandOffset, DETAIL_HIT, framePoints, segmentFill } from "../theme/chromeSpec";
import { formatMapped } from "../theme/tokens";
import { bindEndId, lettersInExpr } from "./bindLinks";
import { bindableArgKeys, handleId } from "./handles";
import { commitBind } from "./bindModel";
import { primaryJackId } from "./connectModel";
import { setCircuitArg } from "./addBlock";
import { bindFace, bindJackXs, chipBox, jackCaption, jackTopPx, LABEL_COL } from "./chipLayout";
import { collapsedFace } from "./chipSpec";
import { detailArgs, isCustomNode, nextCustomInput } from "./detailSchema";
import { isModulatorNode, resolveHz, type ChipData } from "./flowFromAst";
import { lfoPeriodMs, parseLfoShape } from "./lfoLamp";
import { liveArg } from "./liveArg";

function JackPort({
  jack,
  output,
  top,
  showLabel = true,
}: {
  jack: AstJack;
  output: boolean;
  top: number;
  showLabel?: boolean;
}) {
  return (
    <>
      <span className={`nk-jack-shell ${output ? "nk-jack-shell-out" : "nk-jack-shell-in"}`} style={{ top: `${top}px` }} />
      <Handle
        type={output ? "source" : "target"}
        position={output ? Position.Right : Position.Left}
        id={handleId(jack.id, output)}
        className={`nk-plug ${output ? "nk-plug-out" : "nk-plug-in"} ${jack.kind === "mod" ? "nk-plug-mod" : ""}`}
        style={{ top: `${top}px` }}
        isConnectableStart={output}
        isConnectableEnd={! output}
        title={jack.label || jack.id}
      />
      {showLabel ? (
        <span
          className={`nk-plug-label ${output ? "nk-plug-label-out" : "nk-plug-label-in"} ${jack.kind === "mod" ? "nk-plug-label-mod" : ""}`}
          style={{ top: `${top}px` }}
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
            <span className="nk-bind-jack-cap">{(bound || row.key).slice(0, 4).toUpperCase()}</span>
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
  const dragging = useBindStore((s) => s.letter);
  const knobs = useHostStore((s) => s.knobs);
  const peak = useTelemetryStore((s) => (d.type === "in" ? s.inPeak : s.outPeak));
  const ins = d.jacks.filter((j) => ! j.output && j.kind !== "knob");
  const outs = d.jacks.filter((j) => j.output && j.kind !== "knob");
  const binds = [...new Set([
    ...bindableArgKeys(d.args),
    ...Object.keys(d.args).filter((k) => lettersInExpr(d.args[k]).length > 0),
  ])];
  const lfo = isModulatorNode({ type: d.type, id });
  const hz = lfo ? resolveHz(d.args.freq ?? d.args.sync ?? "1", knobs) : 0;
  const extra = useMemo(() => detailArgs(isCustomNode(d.type, id) ? "custom" : d.type, d.args), [d.args, d.type, id]);
  const box = chipBox(isCustomNode(d.type, id) ? "custom" : d.type, d.jacks, detail, d.args);
  const liveRows = extra.map((row) => ({ key: row.key, ...liveArg(row.value, knobs), bind: binds.includes(row.key) }));
  const custom = isCustomNode(d.type, id);
  const updateInternals = useUpdateNodeInternals();
  const { setNodes } = useReactFlow();
  const absY = useStore((s) => s.nodeLookup.get(id)?.internals.positionAbsolute.y ?? 0);
  const shape = lfo ? parseLfoShape(d.args.shape ?? d.args.wave ?? "sine") : "sine";
  const lampMs = lfo ? lfoPeriodMs(hz) : 0;
  const expandAt = chipExpandOffset();

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
  const title = face.title;
  const bits = barcodeBits(id);
  const warn = /stage|drive|clip|fold|crush/i.test(d.type + title);
  return (
    <div
      className="nk-chip relative text-[12px]"
      data-node-id={id}
      data-bind-node={id}
      data-focus={focus}
      style={{
        width: box.w,
        height: box.h,
        ["--nk-label-col" as string]: `${LABEL_COL}px`,
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
          stroke="#ff003c"
          strokeWidth="1.15"
          strokeDasharray={CHIP_FRAME_DASH}
          strokeLinejoin="miter"
        />
      </svg>
      <div className="nk-chip-body">
        <div className="flex h-[26px] w-full items-center justify-between gap-2">
          <span className="nk-chip-grip" aria-hidden title="Drag" />
          <span className="nk-chip-title min-w-0 flex-1 truncate text-ink">{title}</span>
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
          <span className="nk-barcode" aria-hidden>
            {bits.map((on, i) => (
              <i key={i} style={{ height: on ? 8 : 3 }} />
            ))}
          </span>
        </div>

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
            {lfo ? `hz ${formatMapped(hz)}` : `peak ${peakToDb(peak).toFixed(1)} dB`}
          </div>
        ) : null}
      </div>

      {binds.length > 0 ? (
        <BindRail
          nodeId={id}
          face={bindFace(absY, box.h)}
          width={box.w}
          rows={liveRows.filter((r) => r.bind)}
        />
      ) : null}

      {ins.map((j, i) => (
        <JackPort key={`in-${j.id}`} jack={j} output={false} top={jackTopPx(i, Math.max(ins.length, 1), box.h)} />
      ))}
      {outs.map((j, i) => (
        <JackPort key={`out-${j.id}`} jack={j} output top={jackTopPx(i, Math.max(outs.length, 1), box.h)} />
      ))}
      {ins.length === 0 && ! lfo ? (
        <JackPort
          jack={{ id: primaryJackId(d.jacks, false), label: "in", output: false, kind: "audio" }}
          output={false}
          top={jackTopPx(0, 1, box.h)}
        />
      ) : null}
      {outs.length === 0 ? (
        <JackPort
          jack={{ id: primaryJackId(d.jacks, true), label: "out", output: true, kind: "audio" }}
          output
          top={jackTopPx(0, 1, box.h)}
        />
      ) : null}
    </div>
  );
}

export function IoNode({ data, id }: NodeProps) {
  const d = data as ChipData;
  const focus = String((data as { focus?: string }).focus ?? "off");
  const isIn = d.type === "in";
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
          stroke={isIn ? "#00f0ff" : "#fcee0a"}
          strokeWidth="1.15"
          strokeDasharray={CHIP_FRAME_DASH}
          strokeLinejoin="miter"
        />
      </svg>
      <div className="relative z-[2] flex w-full items-center justify-center gap-1">
        <span className="nk-chip-title truncate">{d.label}</span>
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
