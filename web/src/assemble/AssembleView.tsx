import { useEffect, useRef, useState } from "react";
import { BoardView } from "./BoardView";
import { SourceInspector } from "./SourceInspector";
import { inspectorDock, SOURCE_INSPECTOR } from "./sourceInspectorLayout";
import { useSourceInspectorStore } from "../store/sourceInspectorStore";

export function AssembleView({
  active = true,
  onOpenTerminal,
}: {
  active?: boolean;
  onOpenTerminal?: () => void;
}) {
  const open = useSourceInspectorStore((s) => s.open);
  const toggle = useSourceInspectorStore((s) => s.toggle);
  const hostRef = useRef<HTMLDivElement>(null);
  const [pane, setPane] = useState({ w: 960, h: 420 });
  useEffect(() => {
    const el = hostRef.current;
    if (! el) {
      return;
    }
    const ro = new ResizeObserver(() => {
      setPane({ w: el.clientWidth, h: el.clientHeight });
    });
    ro.observe(el);
    setPane({ w: el.clientWidth, h: el.clientHeight });
    return () => ro.disconnect();
  }, []);
  const dock = inspectorDock(open, pane.w, pane.h);
  const side = dock === "side";
  return (
    <div
      ref={hostRef}
      className={`nk-circuit-workspace relative flex h-full min-h-0 w-full ${side ? "flex-row" : "flex-col"}`}
      data-source-dock={dock}
      style={{ ["--nk-source-inspector-width" as string]: `${SOURCE_INSPECTOR.width}px` }}
    >
      <div className="relative min-h-0 min-w-0 flex-1">
        <BoardView active={active} />
        <button
          type="button"
          className="nk-source-toggle nk-clip"
          data-board-control=""
          aria-pressed={open}
          onClick={() => toggle()}
        >
          Source
        </button>
      </div>
      {dock !== "closed" ? (
        <SourceInspector
          dock={dock}
          paneW={pane.w}
          paneH={pane.h}
          active={active}
          onOpenTerminal={() => onOpenTerminal?.()}
        />
      ) : null}
    </div>
  );
}
