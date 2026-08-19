export interface OptimizeReport {
  script: string;
  original: string;
  changes: number;
  messages: string[];
}

function compact(s: string): string {
  return s.replace(/\s+/g, "");
}

const WHOLE: Array<{ from: string; to: string; msg: string }> = [
  { from: "x*1", to: "x", msg: "Drop * 1" },
  { from: "1*x", to: "x", msg: "Drop 1 *" },
  { from: "x*1.0", to: "x", msg: "Drop * 1.0" },
  { from: "x+0", to: "x", msg: "Drop + 0" },
  { from: "0+x", to: "x", msg: "Drop 0 +" },
  { from: "x-0", to: "x", msg: "Drop - 0" },
  { from: "x/1", to: "x", msg: "Drop / 1" },
  { from: "x/1.0", to: "x", msg: "Drop / 1.0" },
  { from: "--x", to: "x", msg: "Drop double negate" },
  { from: "-(-x)", to: "x", msg: "Drop double negate" },
  { from: "x/(1+abs(x))", to: "softclip(x)", msg: "Use softclip(x)" },
  { from: "clamp(x,-1,1)", to: "hardclip(x,1)", msg: "Use hardclip(x, 1)" },
  { from: "min(1,max(-1,x))", to: "hardclip(x,1)", msg: "Use hardclip(x, 1)" },
];

export function optimizeScript(source: string): OptimizeReport {
  const messages: string[] = [];
  let changes = 0;
  const out = source.split("\n").map((line) => {
    const hash = line.indexOf("#");
    const code = hash >= 0 ? line.slice(0, hash) : line;
    const comment = hash >= 0 ? line.slice(hash) : "";
    const eq = code.indexOf("=");
    if (eq < 0) {
      return line;
    }
    const left = code.slice(0, eq + 1);
    let expr = code.slice(eq + 1);
    const packed = compact(expr);
    for (const rule of WHOLE) {
      if (packed === rule.from) {
        expr = ` ${rule.to} `;
        changes += 1;
        messages.push(rule.msg);
        break;
      }
    }
    return `${left}${expr}${comment}`;
  }).join("\n");
  return { script: out, original: source, changes, messages };
}

export function optimizeShowsApply(report: OptimizeReport): boolean {
  return report.changes > 0;
}

export function optimizeEmptyMessage(report: OptimizeReport): string {
  if (report.changes === 0) {
    return "Script already optimal";
  }
  return `${report.changes} safe rewrite(s). Review and Apply.`;
}
