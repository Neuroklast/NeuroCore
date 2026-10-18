import type { ComponentType } from "react";
import type { FaceKind, FaceProps } from "./faceKind";
import { DelayFace } from "./DelayFace";
import { DriveFace } from "./DriveFace";
import { DynamicsFace } from "./DynamicsFace";
import { EmptyFace } from "./EmptyFace";
import { EnvFace } from "./EnvFace";
import { FilterFace } from "./FilterFace";
import { LfoFace } from "./LfoFace";
import { PitchFace } from "./PitchFace";
import { ReverbFace } from "./ReverbFace";

const FACES: Partial<Record<FaceKind, ComponentType<FaceProps>>> = {
  filter: FilterFace,
  reverb: ReverbFace,
  delay: DelayFace,
  lfo: LfoFace,
  dynamics: DynamicsFace,
  env: EnvFace,
  pitch: PitchFace,
  drive: DriveFace,
};

/** Family renderer. Add faces here; do not switch on type names in BoardChip. */
export function NodeFace({ kind, type, args }: FaceProps) {
  const Face = FACES[kind] ?? EmptyFace;
  return <Face kind={kind} type={type} args={args} />;
}
