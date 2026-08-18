export type Workspace = "face" | "assemble" | "hack";

export const WORKSPACES: Array<{ id: Workspace; label: string }> = [
  { id: "face", label: "Unit" },
  { id: "assemble", label: "Circuit" },
  { id: "hack", label: "Terminal" },
];

export function isOpenBoard(w: Workspace): boolean {
  return w === "assemble" || w === "hack";
}

/** Letter-plug drag only exists where there are chips to drop on. */
export function knobBindEnabled(w: Workspace): boolean {
  return w === "assemble";
}

/** Circuit parks the six knobs on a bottom rail; Terminal keeps the left stack. */
export function knobRail(w: Workspace): "left" | "bottom" | "none" {
  if (w === "assemble") {
    return "bottom";
  }
  if (w === "hack") {
    return "left";
  }
  return "none";
}

export type TerminalAction = "edit" | "validate" | "optimize";

/** Script tools that live on the Terminal chrome, not only the header. */
export function terminalActions(): TerminalAction[] {
  return ["edit", "validate", "optimize"];
}
