import { applyMetadata } from "./metadata.mjs";

export function applyOverhaul(input) {
  return input.map(applyMetadata);
}
