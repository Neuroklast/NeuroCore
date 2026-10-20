/** Depth of a Circuit chip. Paint only; AABB and jack locals stay put. */
export function chipDepth(state: {
  selected: boolean;
  focus?: "off" | "soft" | "sharp";
  bindOpen: boolean;
}): 1 | 2 | 3 {
  if (state.bindOpen) {
    return 3;
  }
  if (state.selected || state.focus === "sharp") {
    return 2;
  }
  return 1;
}
