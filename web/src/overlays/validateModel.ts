import type { Diagnostic } from "../bridge/ast";

export interface ValidateIssue {
  severity: "error" | "warn";
  message: string;
}

export interface ValidateReport {
  ok: boolean;
  issues: ValidateIssue[];
}

export function validateScript(script: string, diagnostics: Diagnostic[]): ValidateReport {
  const issues: ValidateIssue[] = [];
  const text = script.trim();
  if (! text) {
    issues.push({ severity: "error", message: "Empty script." });
  }
  const opens = (text.match(/\{/g) ?? []).length;
  const closes = (text.match(/\}/g) ?? []).length;
  if (opens !== closes) {
    issues.push({ severity: "error", message: "Unbalanced { }." });
  }
  const parens = (text.match(/\(/g) ?? []).length - (text.match(/\)/g) ?? []).length;
  if (parens !== 0) {
    issues.push({ severity: "error", message: "Unbalanced ( )." });
  }
  if (/\bNaN\b|\bInf\b|1\s*\/\s*0/i.test(text)) {
    issues.push({ severity: "warn", message: "Possible NaN / Inf in the script." });
  }
  for (const d of diagnostics) {
    issues.push({ severity: "error", message: `line ${d.line}: ${d.message}` });
  }
  return { ok: issues.every((i) => i.severity !== "error"), issues };
}
