import type { ComponentType } from "react";
import type { FaceKind, FaceProps } from "./faceKind";
import { DelayFace } from "./DelayFace";
import { DynamicsFace } from "./DynamicsFace";
import { EmptyFace } from "./EmptyFace";
import { FilterFace } from "./FilterFace";
import { LfoFace } from "./LfoFace";
import { ReverbFace } from "./ReverbFace";

const FACES: Partial<Record<FaceKind, ComponentType<FaceProps>>> = {
  filter: FilterFace,
  reverb: ReverbFace,
  delay: DelayFace,
  lfo: LfoFace,
  dynamics: DynamicsFace,
};

/** Family renderer. Add faces here; do not switch on type names in BoardChip. */
export function NodeFace({ kind, type, args }: FaceProps) {
  const Face = FACES[kind] ?? EmptyFace;
  return <Face kind={kind} type={type} args={args} />;
}
