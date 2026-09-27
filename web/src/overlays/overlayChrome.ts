/** Overlay sits above OS CRT so Functions/Inspect stay sharp. */
export const OVERLAY_Z = 40;
export const CRT_Z = 30;

/** Split explorers: left rail stays put, only the list pane scrolls. */
export function overlayIsSplit(name: string): boolean {
  return name === "presets" || name === "functions" || name === "stages" || name === "help";
}

export function overlayIsWide(name: string): boolean {
  return overlayIsSplit(name) || name === "about";
}

/** Inspect/settings hug content. Wide explorers keep the 720 px stage. */
export function overlaySizesToContent(name: string): boolean {
  return ! overlayIsWide(name);
}

/** Title X is the closer. Footer Close stacked with it. */
export function overlayShowsHostClose(_name: string): boolean {
  return false;
}

/** Host body: split panels lock overflow so the folder column cannot ride the list. */
export function overlayBodyOverflow(name: string): "hidden" | "auto" {
  return overlayIsSplit(name) ? "hidden" : "auto";
}
