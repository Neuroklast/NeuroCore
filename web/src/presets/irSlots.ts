/** IR slot ids live in the script. The WAV is host state, never the formula. */

export type IrSlotView = {
  slot: string;
  name: string;
  loaded: boolean;
};

export function isIrSlotId(id: string): boolean {
  const t = id.trim().toLowerCase();
  if (t === "ir" || t === "convolve") {
    return true;
  }
  if (t.startsWith("ir") && t.length > 2 && /^[0-9]+$/.test(t.slice(2))) {
    return true;
  }
  if (t.startsWith("convolve") && t.length > 8 && /^[0-9]+$/.test(t.slice(8))) {
    return true;
  }
  return false;
}

export function irSlotFromLine(line: string): string | null {
  const t = line.trimStart();
  if (t.startsWith("#") || t.startsWith("//")) {
    return null;
  }
  const colon = t.indexOf(":");
  if (colon <= 0) {
    return null;
  }
  const id = t.slice(0, colon).trim().toLowerCase();
  return isIrSlotId(id) ? id : null;
}

export function irSlotsFromScript(script: string): string[] {
  const slots: string[] = [];
  for (const line of script.split(/\r?\n/)) {
    const id = irSlotFromLine(line);
    if (id && ! slots.includes(id)) {
      slots.push(id);
    }
  }
  return slots;
}

export function mergeIrSlots(
  scriptSlots: string[],
  hostSlots: Array<{ slot: string; name: string; loaded: boolean }>,
): IrSlotView[] {
  const bySlot = new Map<string, IrSlotView>();
  for (const s of hostSlots) {
    const slot = s.slot.trim().toLowerCase();
    if (! slot) {
      continue;
    }
    bySlot.set(slot, { slot, name: s.name, loaded: s.loaded });
  }
  const out: IrSlotView[] = [];
  const seen = new Set<string>();
  for (const raw of scriptSlots) {
    const slot = raw.trim().toLowerCase();
    if (! slot || seen.has(slot)) {
      continue;
    }
    seen.add(slot);
    out.push(bySlot.get(slot) ?? { slot, name: "", loaded: false });
  }
  for (const s of hostSlots) {
    const slot = s.slot.trim().toLowerCase();
    if (! slot || seen.has(slot)) {
      continue;
    }
    seen.add(slot);
    out.push({ slot, name: s.name, loaded: s.loaded });
  }
  return out;
}

/** Circuit / Stages: an IR chip opens Impulse. Everything else stays Inspect. */
export function chipOverlay(nodeId: string, nodeType: string): { overlay: "ir" | "inspect"; inspectId: string } {
  return isIrSlotId(nodeId) || isIrSlotId(nodeType)
    ? { overlay: "ir", inspectId: nodeId }
    : { overlay: "inspect", inspectId: nodeId };
}
