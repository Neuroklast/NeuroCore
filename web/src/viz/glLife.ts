export type GlLife = "live" | "lost";

export function glOnLost(life: GlLife): GlLife {
  return life === "live" ? "lost" : life;
}

export function glOnRestored(life: GlLife): GlLife {
  return life === "lost" ? "live" : life;
}

export function glShouldDraw(life: GlLife): boolean {
  return life === "live";
}
