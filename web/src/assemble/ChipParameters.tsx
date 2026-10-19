import { useBindStore } from "../store/telemetryStore";
import { boundLetters } from "./BindRail";
import { bindEndId } from "./bindLinks";
import { commitBind } from "./bindModel";

export type ChipParameterRow = { key: string; label: string; x?: number; formula: string; bindable: boolean };

/** Macro binding owns the caption; modulation owns the separate edge pin. */
export function ChipParameterCaption({ nodeId, row }: { nodeId: string; row: ChipParameterRow }) {
  const dragging = useBindStore(s => s.letter);
  const letters = boundLetters(row.formula);
  const label = row.label.toUpperCase();
  return <button type="button"
    className={`nk-chip-parameter nodrag nopan ${letters.length ? "is-bound" : ""} ${row.x == null ? "is-inline" : ""}`}
    style={row.x == null ? undefined : { left: row.x }} title={label}
    aria-label={`${row.bindable ? "Bind" : "Parameter"} ${label}`}
    disabled={!row.bindable} data-chip-keep-open=""
    data-bind-node={row.bindable ? nodeId : undefined} data-bind-key={row.bindable ? row.key : undefined}
    onClick={e => { e.stopPropagation(); if (dragging && row.bindable) { commitBind(nodeId, row.key, dragging); useBindStore.getState().end(); } }}>
    {label}
    {letters.map(letter => <span key={letter} className="nk-bind-end" data-bind-end={bindEndId(letter, nodeId)} />)}
  </button>;
}

/** Full captions and macro endpoints share a reserved band above the status foot. */
export function ChipParameters({ nodeId, rows, hidden }: {
  nodeId: string; rows: ChipParameterRow[]; hidden: boolean;
}) {
  return <div className="nk-chip-parameters" style={hidden ? { visibility: "hidden" } : undefined}>
    {rows.map(row => <ChipParameterCaption key={row.key} nodeId={nodeId} row={row} />)}
  </div>;
}
