import { BOARD_BLOCK, BOARD_GRID, BOARD_TRACE, snapSize, snapToCellCenter } from "./grid";

/** Catalog max side rails (msplit low/mid/high). Plates grow past this. */
export const SIDE_JACK_MAX = 3;

/** Stable name row. Controls own the separate technical row. */
export const TITLE_H = 26;
export const TYPECODE_H = 14;
/** Identification, controls, signal surface and captions have separate bands. */
export const TECHNICAL_H = 26;
export const PARAMETER_H = 26;
export const PARAMETER_FONT_PX = 17;
/** Conservative mono glyph plus tracking, shared by caption and inline bind sizing. */
export const PARAMETER_CHAR_PX = 10.5;
/** One socket is title + typecode stacked (key + value). Overlay rows. */
export const SOCKET_H = TITLE_H + TYPECODE_H;
export const CHIP_PAD_Y = TITLE_H - TYPECODE_H;
export const CHIP_PAD_X = BOARD_TRACE;
/** Overlay gutter. Closed IC plates use PIN_LEAD, not this column. */
export const LABEL_COL = BOARD_GRID + CHIP_PAD_Y;
export const RULE_H = CHIP_PAD_Y;
export const MS_ROW = TITLE_H;
export const SOCK_GAP = CHIP_PAD_Y - BOARD_TRACE / 2;
export const BIND_RAIL = BOARD_GRID;
/** South rail leftover for overlay captions. Closed foot is FOOT_H. */
export const SOUTH_JACK_GAP = BOARD_GRID + TITLE_H;
export const CHAR_PX = 9;
export const VALUE_FIELD_PAD = BOARD_GRID + BOARD_TRACE / 2;
/** Closed-face well floor. Overlay holds long enum fields. */
export const CONTENT_MIN = BOARD_GRID;

/** QFP lead from AABB edge to gunmetal body. Jack centres stay on the AABB. */
export const PIN_LEAD = BOARD_TRACE / 2;
/** Status foot on the body, above the south pin strip. */
export const FOOT_H = BOARD_TRACE;
/** Baseline DSP IC. Size grows from jack counts; never shrinks to fit a row. */
export const CHIP_W = BOARD_GRID * 11;
export const CHIP_H = BOARD_GRID * 8;
export const IO_H = BOARD_GRID * 5;
/** ui-ux: IN/OUT body ≥ 96. Compact connector housing; arrangement aligns the actual jack centres. */
export const IO_W = BOARD_GRID * 5;
export const BIND_JACK_MIN = BOARD_GRID * 2;
export const OVERLAY_MAX_H = BOARD_BLOCK * 2;
export const PLUG_LABEL_INSET = BOARD_TRACE;
export const JACK_PITCH = BOARD_GRID;
export const FORK_PITCH = BOARD_GRID * 2;
export const FACE_MIN_W = CHIP_W - PIN_LEAD * 2;
export const FACE_MIN_H = CHIP_H - PIN_LEAD * 2 - TITLE_H - TECHNICAL_H - PARAMETER_H - FOOT_H;

/** Gunmetal body inside the AABB pin strip. */
export function icBodyRect(box: { w: number; h: number }): { x: number; y: number; w: number; h: number } {
  return {
    x: PIN_LEAD,
    y: PIN_LEAD,
    w: Math.max(0, box.w - PIN_LEAD * 2),
    h: Math.max(0, box.h - PIN_LEAD * 2),
  };
}

/** Signal band: below identification/controls, above captions and status. */
export function sideJackBand(boxH: number, role: "io" | "chip" = "chip"): { top: number; innerH: number; mid: number } {
  const top = PIN_LEAD + TITLE_H + TECHNICAL_H;
  const innerH = Math.max(0, boxH - top - PIN_LEAD - FOOT_H - (role === "io" ? 0 : PARAMETER_H));
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
  return snapToCellCenter(PIN_LEAD + TITLE_H);
}

export function chipFaceStackPx(): number {
  return CHIP_H;
}

export function icStackHeight(sideCount: number, minH = CHIP_H): number {
  const n = Math.max(1, Math.floor(Number(sideCount) || 1));
  const chrome = PIN_LEAD * 2 + TITLE_H + TECHNICAL_H + PARAMETER_H + FOOT_H;
  if (n <= 1) {
    return snapSize(Math.max(minH, chrome + FACE_MIN_H));
  }
  const pitch = sideJackPitch(n);
  const inner = (n - 1) * pitch + BOARD_GRID;
  return snapSize(Math.max(minH, chrome + Math.max(FACE_MIN_H, inner)));
}

export function icStackWidth(southCount: number, minW = CHIP_W, pitch = JACK_PITCH): number {
  const n = Math.max(0, Math.floor(Number(southCount) || 0));
  if (n <= 1) {
    return snapSize(minW);
  }
  return snapSize(Math.max(minW, PIN_LEAD * 2 + n * pitch));
}

export function dspFaceSize(): { w: number; h: number } {
  return { w: snapSize(CHIP_W), h: snapSize(CHIP_H) };
}

export function ioFaceSize(jackCount = 1): { w: number; h: number } {
  const n = Math.max(1, Math.floor(Number(jackCount) || 1));
  const chrome = PIN_LEAD * 2 + TITLE_H + TECHNICAL_H + FOOT_H;
  return { w: snapSize(IO_W), h: snapSize(Math.max(IO_H, chrome + n * JACK_PITCH)) };
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
    top: PIN_LEAD,
    right: PIN_LEAD,
    bottom: PIN_LEAD + FOOT_H,
    left: PIN_LEAD,
  };
}

/** Utility IN/OUT: pin strip only. */
export function ioBodyInset(): { top: number; right: number; bottom: number; left: number } {
  return {
    top: PIN_LEAD,
    right: PIN_LEAD,
    bottom: PIN_LEAD + FOOT_H,
    left: PIN_LEAD,
  };
}

/** Dotted signal surface between the control row and parameter/status bands. */
export function chipPadInset(role: "io" | "chip"): {
  top: number;
  right: number;
  bottom: number;
  left: number;
} {
  return {
    top: PIN_LEAD + TITLE_H + TECHNICAL_H,
    right: PIN_LEAD,
    bottom: PIN_LEAD + FOOT_H + (role === "io" ? 0 : PARAMETER_H),
    left: PIN_LEAD,
  };
}

export function chipChromeVars(role: "io" | "chip" = "chip"): Record<string, string> {
  const pad = chipPadInset(role);
  return {
    "--nk-label-col": `${LABEL_COL}px`,
    "--nk-chip-pad-x": `${CHIP_PAD_X}px`,
    "--nk-chip-pad-y": `${CHIP_PAD_Y}px`,
    "--nk-title-h": `${TITLE_H}px`,
    "--nk-technical-h": `${TECHNICAL_H}px`,
    "--nk-parameter-h": `${PARAMETER_H}px`,
    "--nk-parameter-font": `${PARAMETER_FONT_PX}px`,
    "--nk-char-px": `${CHAR_PX}px`,
    "--nk-socket-h": `${SOCKET_H}px`,
    "--nk-sock-gap": `${SOCK_GAP}px`,
    "--nk-south-gap": `${SOUTH_JACK_GAP}px`,
    "--nk-pin-lead": `${PIN_LEAD}px`,
    "--nk-foot-h": `${FOOT_H}px`,
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
