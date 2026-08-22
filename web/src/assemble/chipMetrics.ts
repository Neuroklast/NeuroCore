import { BOARD_BLOCK, BOARD_GRID, BOARD_HALF, BOARD_TRACE, snapSize, snapToCellCenter } from "./grid";

/** Catalog max side rails (msplit low/mid/high) so every DSP plate fits the same forks. */
export const SIDE_JACK_MAX = 3;

/** ui-ux: hit targets ≥ 26 px. Title band is that plate. */
export const TITLE_H = 26;
export const TYPECODE_H = 14;
/** One socket is title + typecode stacked (key + value). */
export const SOCKET_H = TITLE_H + TYPECODE_H;
export const CHIP_PAD_Y = TITLE_H - TYPECODE_H;
export const CHIP_PAD_X = BOARD_TRACE;
/** Side gutter: one cell plus pad, so IN/OUT labels never sit on typecode. */
export const LABEL_COL = BOARD_GRID + CHIP_PAD_Y;
export const RULE_H = CHIP_PAD_Y;
export const MS_ROW = TITLE_H;
export const SOCK_GAP = CHIP_PAD_Y - BOARD_TRACE / 2;
export const BIND_RAIL = BOARD_GRID;
/** South rail: jack cell + caption the size of the title. */
export const SOUTH_JACK_GAP = BOARD_GRID + TITLE_H;
export const CHAR_PX = 9;
export const VALUE_FIELD_PAD = BOARD_GRID + BOARD_TRACE / 2;
export const CONTENT_MIN = BOARD_BLOCK + BOARD_TRACE;
export const CHIP_W = LABEL_COL * 2 + CONTENT_MIN;
export const CHIP_H = BOARD_GRID * 2 + BOARD_TRACE;
export const IO_H = BOARD_GRID * 3;
export const IO_W = LABEL_COL + CHIP_H;
export const BIND_JACK_MIN = BOARD_GRID * 2;
export const OVERLAY_MAX_H = BOARD_BLOCK * 2;
export const PLUG_LABEL_INSET = BOARD_TRACE;
export const JACK_PITCH = BOARD_GRID;
export const FORK_PITCH = BOARD_GRID * 2;

/** Body band: below the title, above the foot. Side jacks live here. */
export function sideJackBand(boxH: number): { top: number; innerH: number; mid: number } {
  const top = TITLE_H;
  const innerH = Math.max(0, boxH - TITLE_H - CHIP_PAD_Y);
  return { top, innerH, mid: snapToCellCenter(top + innerH * 0.5) };
}

/** Fork pitch when it fits the body band; otherwise one cell so IN out/sc stay off the title and foot. */
export function sideJackPitch(count: number, innerH = Number.POSITIVE_INFINITY): number {
  if (count < 2) {
    return JACK_PITCH;
  }
  return (count - 1) * FORK_PITCH <= innerH ? FORK_PITCH : JACK_PITCH;
}

export function longestValuePx(enums: Record<string, string[]>): number {
  let n = 8;
  for (const list of Object.values(enums)) {
    for (const w of list) {
      n = Math.max(n, w.length);
    }
  }
  return n * CHAR_PX + VALUE_FIELD_PAD;
}

/** Side IN/OUT sit under the title/chevron plate, in the typecode band. */
export function titleJackY(): number {
  return snapToCellCenter(CHIP_PAD_Y + TITLE_H);
}

export function chipFaceStackPx(): number {
  const identity = CHIP_PAD_Y + TITLE_H + TYPECODE_H + RULE_H + MS_ROW + SOUTH_JACK_GAP;
  const rails = titleJackY() + (SIDE_JACK_MAX - 1) * FORK_PITCH + BOARD_GRID + BOARD_HALF;
  return Math.max(identity, rails);
}

export function dspFaceSize(): { w: number; h: number } {
  return { w: snapSize(CHIP_W), h: snapSize(chipFaceStackPx()) };
}

export function ioFaceSize(): { w: number; h: number } {
  return { w: snapSize(IO_W), h: snapSize(IO_H) };
}

export function chipOverlayStackPx(hiddenRows: number): number {
  if (hiddenRows <= 0) {
    return 0;
  }
  const socks = hiddenRows * SOCKET_H + Math.max(0, hiddenRows - 1) * SOCK_GAP;
  return CHIP_PAD_Y + socks + CHIP_PAD_Y;
}

export function chipBodyInset(): { top: number; right: number; bottom: number; left: number } {
  return {
    top: CHIP_PAD_Y,
    right: LABEL_COL,
    bottom: SOUTH_JACK_GAP,
    left: LABEL_COL,
  };
}

/** Utility IN/OUT: pad only. DSP label-column gutters clip "OUT". */
export function ioBodyInset(): { top: number; right: number; bottom: number; left: number } {
  return {
    top: CHIP_PAD_Y,
    right: CHIP_PAD_X,
    bottom: CHIP_PAD_Y,
    left: CHIP_PAD_X,
  };
}

/** Empty plate under typecode/M-S and above the south rail — the pad array lives here. */
export function chipPadInset(role: "io" | "chip"): {
  top: number;
  right: number;
  bottom: number;
  left: number;
} {
  if (role === "io") {
    return { top: TITLE_H, right: CHIP_PAD_X, bottom: CHIP_PAD_Y, left: CHIP_PAD_X };
  }
  return {
    top: TITLE_H + TYPECODE_H + RULE_H + MS_ROW,
    right: LABEL_COL,
    bottom: SOUTH_JACK_GAP + TYPECODE_H,
    left: LABEL_COL,
  };
}

export function chipChromeVars(role: "io" | "chip" = "chip"): Record<string, string> {
  const pad = chipPadInset(role);
  return {
    "--nk-label-col": `${LABEL_COL}px`,
    "--nk-chip-pad-x": `${CHIP_PAD_X}px`,
    "--nk-chip-pad-y": `${CHIP_PAD_Y}px`,
    "--nk-title-h": `${TITLE_H}px`,
    "--nk-char-px": `${CHAR_PX}px`,
    "--nk-socket-h": `${SOCKET_H}px`,
    "--nk-sock-gap": `${SOCK_GAP}px`,
    "--nk-south-gap": `${SOUTH_JACK_GAP}px`,
    "--nk-pad-top": `${pad.top}px`,
    "--nk-pad-right": `${pad.right}px`,
    "--nk-pad-bottom": `${pad.bottom}px`,
    "--nk-pad-left": `${pad.left}px`,
    "--nk-plug-label-inset": `${PLUG_LABEL_INSET}px`,
    "--nk-overlay-max": `${OVERLAY_MAX_H}px`,
  };
}

export function isUtilityIo(id: string): boolean {
  return id === "in" || id === "out" || id === "sidechain";
}

export const CHIP_KEEP_OPEN_SEL = "[data-chip-keep-open],[data-knob-bind],[data-bind-key],[data-chip-expand]";
