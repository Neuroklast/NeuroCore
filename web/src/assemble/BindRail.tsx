import { CHIP_CLIP, DETAIL_HIT } from "../theme/chromeSpec";
import { useBindStore } from "../store/telemetryStore";
import { bindEndId } from "./bindLinks";
import { commitBind } from "./bindModel";
import { bindCaptionMaxPx, bindJackCaption, bindJackXs, CHIP_PAD_X } from "./chipLayout";

export function bindGhostHot(): { x: number; y: number } {
  return { x: 0, y: 0 };
}

export function bindPadCols(n: number): number {
  if (n <= 1) {
    return 1;
  }
  if (n <= 4) {
    return 2;
  }
  if (n <= 6) {
    return 3;
  }
  return 4;
}

export type BindPadCell = { i: number; x: number; y: number; w: number; h: number };

/** Pack n drop pads inside the chip box. Gap is one trace. */
export function bindPadCells(
  n: number,
  box: { w: number; h: number },
  pad = CHIP_PAD_X,
): BindPadCell[] {
  if (n <= 0) {
    return [];
  }
  const gap = 6;
  const cols = bindPadCols(n);
  const rows = Math.ceil(n / cols);
  const innerW = Math.max(DETAIL_HIT, box.w - pad * 2);
  const innerH = Math.max(DETAIL_HIT, box.h - pad * 2);
  const cw = innerW / cols;
  const ch = innerH / rows;
  const out: BindPadCell[] = [];
  for (let i = 0; i < n; i += 1) {
    const c = i % cols;
    const r = Math.floor(i / cols);
    out.push({
      i,
      x: pad + c * cw + gap,
      y: pad + r * ch + gap,
      w: cw - gap * 2,
      h: ch - gap * 2,
    });
  }
  return out;
}

export function bindCellAt(local: { x: number; y: number }, cells: BindPadCell[]): number {
  return cells.findIndex((c) => (
    local.x >= c.x && local.x < c.x + c.w && local.y >= c.y && local.y < c.y + c.h
  ));
}

export function boundLetters(formula: string): string[] {
  const exact = formula.trim().toLowerCase();
  if (/^[a-f]$/.test(exact)) {
    return [exact];
  }
  const out: string[] = [];
  for (const L of formula.match(/\b[a-f]\b/gi) ?? []) {
    const n = L.toLowerCase();
    if (! out.includes(n)) {
      out.push(n);
    }
  }
  return out;
}

export function BindRail({
  nodeId,
  face,
  width,
  rows,
}: {
  nodeId: string;
  face: "top" | "bottom";
  width: number;
  rows: Array<{ key: string; formula: string }>;
}) {
  const xs = bindJackXs(rows.length, width);
  const capW = bindCaptionMaxPx(rows.length, width);
  const dragging = useBindStore((s) => s.letter);
  return (
    <div className="nk-bind-rail" data-bind-face={face} data-chip-keep-open="" style={face === "top" ? { top: 0 } : { bottom: 0 }}>
      {rows.map((row, i) => {
        const letters = boundLetters(row.formula);
        return (
          <button
            key={row.key}
            type="button"
            className={`nk-bind-jack nodrag ${letters.length ? "on" : ""} ${dragging ? "hot" : ""}`}
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

/** In-chip drop pads while a knob is dragged over this DSP block. */
export function BindDropPad({
  nodeId,
  keys,
  args,
  w,
  h,
  local,
}: {
  nodeId: string;
  keys: string[];
  args: Record<string, string>;
  w: number;
  h: number;
  local: { x: number; y: number };
}) {
  const dragging = useBindStore((s) => s.letter);
  const cells = bindPadCells(keys.length, { w, h });
  const hi = bindCellAt(local, cells);
  const hot = hi >= 0 ? keys[hi]! : null;
  if (keys.length === 0) {
    return null;
  }
  return (
    <div
      className="nk-bind-pad nodrag nopan"
      data-bind-overlay=""
      data-chip-keep-open=""
      style={{
        position: "absolute",
        left: 0,
        top: 0,
        width: w,
        height: h,
        zIndex: 20,
        background: "#050508",
        clipPath: CHIP_CLIP,
      }}
    >
      {cells.map((c) => {
        const key = keys[c.i]!;
        const letters = boundLetters(args[key] ?? "");
        const on = hot === key;
        return (
          <div
            key={key}
            role="button"
            className={`nk-bind-cell ${letters.length ? "is-wired" : ""} ${on ? "is-hot" : ""}`}
            data-bind-drop=""
            data-bind-node={nodeId}
            data-bind-key={key}
            style={{
              position: "absolute",
              left: c.x,
              top: c.y,
              width: Math.max(c.w, 1),
              height: Math.max(c.h, 1),
              display: "flex",
              flexDirection: "column",
              alignItems: "center",
              justifyContent: "center",
              gap: 6,
              background: on ? "#1a3040" : "#181822",
              border: "2px solid #00f0ff",
              boxShadow: on ? "0 0 16px #00f0ff, inset 0 0 12px rgba(0,240,255,0.35)" : "none",
              boxSizing: "border-box",
            }}
            onClick={(e) => {
              e.stopPropagation();
              if (dragging) {
                commitBind(nodeId, key, dragging);
                useBindStore.getState().end();
              }
            }}
          >
            <i className="nk-bind-well" aria-hidden />
            <span className="nk-bind-cell-key">{bindJackCaption(key)}</span>
            {letters[0] ? <span className="nk-bind-cell-letter">{letters[0]}</span> : null}
            {letters.map((L) => (
              <span key={L} data-bind-end={bindEndId(L, nodeId)} className="nk-bind-end" />
            ))}
          </div>
        );
      })}
    </div>
  );
}
