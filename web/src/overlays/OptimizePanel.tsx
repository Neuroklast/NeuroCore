import { publishScript } from "../assemble/addBlock";
import { useAstStore } from "../store/astStore";
import {
  optimizeEmptyMessage,
  optimizeScript,
  optimizeShowsApply,
} from "./optimizeModel";
import { validateOnSave } from "./validateModel";

export function OptimizePanel() {
  const script = useAstStore((s) => s.lastValidScript || s.script);
  const diagnostics = useAstStore((s) => s.diagnostics);
  const report = optimizeScript(script);
  const showApply = optimizeShowsApply(report);
  return (
    <div className="flex flex-col gap-3 text-[12px]">
      <p className="text-[13px] text-ink">{optimizeEmptyMessage(report)}</p>
      {report.messages.map((m) => (
        <div key={m} className="text-accent">· {m}</div>
      ))}
      {showApply ? (
        <>
          <div className="grid grid-cols-2 gap-2">
            <div>
              <div className="mb-1 text-[11px] tracking-widest text-muted">ORIGINAL</div>
              <pre className="max-h-48 overflow-auto border border-accent/30 bg-black p-2 text-ink">{report.original}</pre>
            </div>
            <div>
              <div className="mb-1 text-[11px] tracking-widest text-muted">OPTIMIZED</div>
              <pre className="max-h-48 overflow-auto border border-accent/30 bg-black p-2 text-accent">{report.script}</pre>
            </div>
          </div>
          <button
            type="button"
            className="nk-clip self-start"
            onClick={() => {
              validateOnSave(report.script, diagnostics);
              publishScript(report.script, "editor");
            }}
          >
            Apply
          </button>
        </>
      ) : null}
    </div>
  );
}
