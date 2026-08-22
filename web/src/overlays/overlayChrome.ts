/** Split explorers: left rail stays put, only the list pane scrolls. */
export function overlayIsSplit(name: string): boolean {
  return name === "presets" || name === "functions" || name === "stages" || name === "help";
}

export function overlayIsWide(name: string): boolean {
  return overlayIsSplit(name) || name === "about";
}

/** Preset explorer already has Load / Close. Host footer Close would stack. */
export function overlayShowsHostClose(name: string): boolean {
  return name !== "presets" && name !== "discard";
}

/** Host body: split panels lock overflow so the folder column cannot ride the list. */
export function overlayBodyOverflow(name: string): "hidden" | "auto" {
  return overlayIsSplit(name) ? "hidden" : "auto";
}
