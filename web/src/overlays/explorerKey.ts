export type ExplorerKeyZone = "search" | "list" | "field" | "button" | "other";
export type ExplorerKeyIntent = "none" | "move-up" | "move-down" | "load" | "close" | "cancel-save";

export function explorerKeyZone(flags: {
  inSearch?: boolean;
  inField?: boolean;
  inList?: boolean;
  inButton?: boolean;
  inMonaco?: boolean;
}): ExplorerKeyZone {
  if (flags.inMonaco || (flags.inField && ! flags.inSearch)) {
    return "field";
  }
  if (flags.inSearch) {
    return "search";
  }
  if (flags.inList) {
    return "list";
  }
  if (flags.inButton) {
    return "button";
  }
  return "other";
}

export function explorerKeyIntent(key: string, zone: ExplorerKeyZone, saveOpen = false): ExplorerKeyIntent {
  if (key === "Escape") {
    return saveOpen ? "cancel-save" : "close";
  }
  if (zone === "field" || zone === "button" || zone === "other") {
    return "none";
  }
  if (key === "Enter" && (zone === "search" || zone === "list")) {
    return "load";
  }
  if (zone === "list" && key === "ArrowDown") {
    return "move-down";
  }
  if (zone === "list" && key === "ArrowUp") {
    return "move-up";
  }
  return "none";
}

export function explorerKeyFlags(target: EventTarget | null): Parameters<typeof explorerKeyZone>[0] {
  const el = target as HTMLElement | null;
  if (! el?.closest) {
    return {};
  }
  return {
    inSearch: Boolean(el.closest("#preset-search")),
    inField: Boolean(el.closest("input, textarea, select, [contenteditable]")),
    inList: Boolean(el.closest(".nk-preset-list")),
    inButton: Boolean(el.closest("button, a")),
    inMonaco: Boolean(el.closest(".monaco-editor")),
  };
}
