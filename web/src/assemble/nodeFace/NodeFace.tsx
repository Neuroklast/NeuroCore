import type { ComponentType } from "react";
import type { FaceKind, FaceProps } from "./faceKind";
import { CabFace } from "./CabFace";
import { DelayFace } from "./DelayFace";
import { DriveFace } from "./DriveFace";
import { DynamicsFace } from "./DynamicsFace";
import { EmptyFace } from "./EmptyFace";
import { EnvFace } from "./EnvFace";
import { FilterFace } from "./FilterFace";
import { IoFace } from "./IoFace";
import { LfoFace } from "./LfoFace";
import { ModfxFace } from "./ModfxFace";
import { PitchFace } from "./PitchFace";
import { ReverbFace } from "./ReverbFace";
import { RoutingFace } from "./RoutingFace";
import { UtilityFace } from "./UtilityFace";

const FACES: Partial<Record<FaceKind, ComponentType<FaceProps>>> = {
  filter: FilterFace,
  reverb: ReverbFace,
  delay: DelayFace,
  lfo: LfoFace,
  dynamics: DynamicsFace,
  env: EnvFace,
  pitch: PitchFace,
  drive: DriveFace,
  routing: RoutingFace,
  modfx: ModfxFace,
  cab: CabFace,
  io: IoFace,
  utility: UtilityFace,
};

/** Family renderer. Add faces here; do not switch on type names in BoardChip. */
export function NodeFace({ kind, type, args, id, host }: FaceProps) {
  const Face = FACES[kind] ?? EmptyFace;
  return <Face kind={kind} type={type} args={args} id={id} host={host} />;
}
