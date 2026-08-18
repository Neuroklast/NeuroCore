import { useEffect, useState } from "react";
import { getNativeFunction, hasJuceBridge } from "../bridge/juce";
import { useAstStore } from "../store/astStore";
import { validateScript } from "./validateModel";

export function ValidatePanel() {
  const script = useAstStore((s) => s.script);
  const diagnostics = useAstStore((s) => s.diagnostics);
  const [progress, setProgress] = useState(0);
  const report = validateScript(script, diagnostics);

  useEffect(() => {
    setProgress(0);
    let p = 0;
    const id = window.setInterval(() => {
      p = Math.min(100, p + 8);
      setProgress(p);
      if (p >= 100) {
        window.clearInterval(id);
      }
    }, 40);
    if (hasJuceBridge()) {
      void getNativeFunction("lint")({ script }).catch(() => undefined);
    }
    return () => window.clearInterval(id);
  }, [script]);

  const done = progress >= 100;
  return (
    <div className="flex flex-col gap-3 text-[13px]">
      <p className="text-ink">{done ? (report.ok ? "Stable. No fatal issues." : "Issues found.") : "Validating…"}</p>
      <div className="h-2 border border-accent/50 bg-well">
        <div className="h-full bg-accent" style={{ width: `${progress}%` }} />
      </div>
      {done ? (
        <ul className="flex flex-col gap-1">
          {report.issues.length === 0 ? <li className="text-muted">No warnings.</li> : null}
          {report.issues.map((i, n) => (
            <li key={n} className={i.severity === "error" ? "text-error" : "text-accent"}>
              {i.severity.toUpperCase()} · {i.message}
            </li>
          ))}
        </ul>
      ) : null}
    </div>
  );
}
