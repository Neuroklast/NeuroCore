import { SEMANTIC_ZOOM } from "../assemble/nodeFace/semanticZoom";
import { TITLE_H } from "../assemble/chipMetrics";

/**
 * Industrial Signal Laboratory tokens. Paint still comes from themeOf();
 * these names are the shared geometry and type scale for Circuit / Terminal.
 * Do not add product-wide hex here.
 */
export const SIGNAL_LAB = {
  product: "NEUROKORE",
  version: "0.7.0-beta",
  depth: {
    workspace: 0,
    idle: 1,
    focused: 2,
    interaction: 3,
  },
  type: {
    titlePx: 13,
    paramPx: 11,
    secondaryMinPx: 10,
    codePx: 12,
  },
  hitPx: TITLE_H,
  zoom: SEMANTIC_ZOOM,
  surfaceVar: {
    workspace: "var(--nk-bg)",
    idle: "var(--nk-surface)",
    focused: "var(--nk-surface-high)",
    interaction: "var(--nk-well-high)",
  },
} as const;

export type SignalLabDepth = typeof SIGNAL_LAB.depth[keyof typeof SIGNAL_LAB.depth];
