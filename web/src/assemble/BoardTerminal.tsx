import type { CSSProperties } from "react";
import { useBoardStore } from "./boardStore";
import { useHostStore } from "../store/hostStore";
import { chipExpandAction } from "./boardEdit";
import { portBracket } from "./chipFace";
import { BOARD_HIT, portIsOutput, portLocal, type BoardNode, type BoardPort } from "./boardModel";
import { nodeMeter } from "./nodeMeter";

const LABEL: Record<string, string> = {
  in: "AUDIO IN",
  out: "AUDIO OUT",
  sidechain: "SIDECHAIN IN",
  midi: "MIDI IN",
};

export function BoardTerminal({
  node,
  ports,
  selected,
  focus,
  onInspect,
}: {
  node: BoardNode;
  ports: BoardPort[];
  selected: boolean;
  focus?: "off" | "soft" | "sharp";
  onInspect?: () => void;
}) {
  const showInspect = chipExpandAction(node.role, node.type) === "inspect";
  const inspectOpen = useHostStore((s) => s.inspectId === node.id);
  return (
    <div
      className="nk-board-terminal nk-chip-locked"
      data-node-id={node.id}
      data-terminal={node.type}
      data-kind="io"
      data-io={node.type}
      data-selected={selected ? "on" : "off"}
      data-focus={focus ?? "off"}
      style={{
        position: "absolute",
        left: node.x,
        top: node.y,
        width: node.w,
        height: node.h,
      }}
    >
      <span className="nk-terminal-name">{LABEL[node.type] ?? node.label}</span>
      {node.type === "in" || node.type === "out" || node.type === "sidechain" ? (
        <span className="nk-terminal-lr">L/R</span>
      ) : null}
      {showInspect ? (
        <button
          type="button"
          className="nk-chip-expand nodrag nopan"
          data-chip-keep-open=""
          aria-label={`Inspect ${node.label}`}
          aria-expanded={inspectOpen}
          onClick={(e) => {
            e.stopPropagation();
            onInspect?.();
          }}
        />
      ) : null}
      {ports.map((p) => (
        <TerminalPort key={p.id} node={node} port={p} />
      ))}
    </div>
  );
}

function TerminalPort({ node, port: p }: { node: BoardNode; port: BoardPort }) {
  const sourceId = useBoardStore((s) => {
    const edge = Object.values(s.edges).find((e) => portIsOutput(p) ? e.sourcePortId === p.id : e.targetPortId === p.id);
    return edge?.sourceNodeId ?? (portIsOutput(p) ? node.id : null);
  });
  const sourceType = useBoardStore((s) => sourceId ? s.nodes[sourceId]?.type : undefined);
  const active = useHostStore((s) => {
    if (! sourceId) {
      return false;
    }
    if (p.kind === "mod") {
      return Math.abs(s.mods[sourceId] ?? 0) > 0.001;
    }
    const peak = nodeMeter({ id: sourceId, type: sourceType, clips: s.clips }).peak;
    return peak != null && peak > 0.001;
  });
  const loc = portLocal(node, p);
  return (
    <button
      type="button"
      className={`nk-port nk-port-${p.face === "s" ? "south" : p.face === "n" ? "north" : p.face === "e" ? "east" : "west"} nk-port-${p.kind}`}
      data-port-id={p.id}
      data-tip={portBracket(p.jackId)}
      data-signal={active ? "on" : "off"}
      aria-label={`${portIsOutput(p) ? "Output" : "Input"} ${p.jackId}`}
      style={{ top: loc.y, left: loc.x, width: BOARD_HIT, height: BOARD_HIT } as CSSProperties}
    >
      <i className="nk-port-contact" />
      <span className="nk-port-cap">{portBracket(p.jackId)}</span>
    </button>
  );
}
