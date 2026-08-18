import { publishScript } from "../assemble/addBlock";
import { useAstStore } from "../store/astStore";
import { optimizeScript } from "./optimizeModel";

export function OptimizePanel() {
  const script = useAstStore((s) => s.lastValidScript || s.script);
  const report = optimizeScript(script);
  return (
    <div className="flex flex-col gap-3 text-[12px]">
      <p className="text-[13px] text-ink">
        {report.changes > 0
          ? `${report.changes} safe rewrite(s). Review and Apply.`
          : "No safe rewrites. Script already tight."}
      </p>
      {report.messages.map((m) => (
        <div key={m} className="text-accent">· {m}</div>
      ))}
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
        disabled={report.changes === 0}
        onClick={() => {
          publishScript(report.script, "editor");
        }}
      >
        Apply
      </button>
    </div>
  );
}
