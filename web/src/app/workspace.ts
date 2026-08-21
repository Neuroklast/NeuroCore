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

/** One macro bar: knobs A–F always bottom. Bind plugs only in Circuit. */
export function knobRail(_w: Workspace): "left" | "bottom" | "none" {
  return "bottom";
}

/** Unit keeps ~60 Hz DSP telemetry. Circuit/Terminal only need glow ticks. */
export function telemetryIntervalMs(w: Workspace): number {
  return w === "face" ? 16 : 500;
}

export type TerminalAction = "edit" | "validate" | "optimize";

/** Script tools that live on the Terminal chrome, not only the header. */
export function terminalActions(): TerminalAction[] {
  return ["edit", "validate", "optimize"];
}
