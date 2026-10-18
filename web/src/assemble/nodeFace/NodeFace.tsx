import type { ComponentType } from "react";
import type { FaceKind } from "./faceKind";
import { EmptyFace } from "./EmptyFace";
import { FilterFace } from "./FilterFace";
import { ReverbFace } from "./ReverbFace";

const FACES: Partial<Record<FaceKind, ComponentType>> = {
  filter: FilterFace,
  reverb: ReverbFace,
};

/** Family renderer. Add faces here; do not switch on type names in BoardChip. */
export function NodeFace({ kind }: { kind: FaceKind }) {
  const Face = FACES[kind] ?? EmptyFace;
  return <Face />;
}
