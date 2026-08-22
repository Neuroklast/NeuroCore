import { isEnvNode, isLfoNode } from "./flowFromAst";

export type ChipLamp = "clip" | "lfo" | "env";

/** Closed DSP plate: identity + one lamp. No tech-demo greeble. */
export function closedChipChrome(type: string, id = ""): {
  greeble: boolean;
  barcode: boolean;
  grip: boolean;
  fakeJacks: boolean;
  lamp: ChipLamp;
} {
  const env = isEnvNode({ type, id });
  const lfo = isLfoNode({ type, id });
  return {
    greeble: false,
    barcode: false,
    grip: false,
    fakeJacks: false,
    lamp: env ? "env" : lfo ? "lfo" : "clip",
  };
}

/** Overlay lists the same keys. South captions must not sit under it. */
export function overlayHidesBindRail(detail: boolean): boolean {
  return detail;
}

export function bindRailVisible(detail: boolean, keys: readonly string[]): boolean {
  return keys.length > 0 && ! overlayHidesBindRail(detail);
}
