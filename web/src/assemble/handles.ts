export function handleId(jackId: string, output: boolean): string {
  return `${output ? "src" : "dst"}::${jackId}`;
}

export function parseHandle(handle: string | null | undefined): { id: string; output: boolean } | null {
  if (! handle) {
    return null;
  }
  const split = handle.indexOf("::");
  if (split >= 0) {
    return { id: handle.slice(split + 2), output: handle.startsWith("src") };
  }
  const colon = handle.lastIndexOf(":");
  if (colon < 0) {
    return { id: handle, output: false };
  }
  return { id: handle.slice(0, colon), output: handle.slice(colon + 1) === "out" };
}

export function tokenInExpr(expr: string, token: string): boolean {
  if (! token) {
    return false;
  }
  const s = expr.toLowerCase();
  const t = token.toLowerCase();
  let i = 0;
  while (i <= s.length - t.length) {
    const at = s.indexOf(t, i);
    if (at < 0) {
      return false;
    }
    const prev = at === 0 ? "" : s[at - 1];
    const next = at + t.length >= s.length ? "" : s[at + t.length];
    const word = (ch: string) => /[a-z0-9_]/i.test(ch);
    if (! word(prev) && ! word(next)) {
      return true;
    }
    i = at + 1;
  }
  return false;
}

export function bindableArgKeys(args: Record<string, string>): string[] {
  return Object.keys(args).filter((k) => {
    const low = k.toLowerCase();
    return low !== "y" && low !== "mode" && low !== "channel" && low !== "kanal"
      && low !== "name" && low !== "family";
  });
}

export function applyKnobBind(
  args: Record<string, string>,
  key: string,
  letter: string,
): Record<string, string> {
  if (! key || ! /^[a-f]$/i.test(letter)) {
    return args;
  }
  return { ...args, [key]: letter.toLowerCase() };
}
