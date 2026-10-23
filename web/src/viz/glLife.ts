export type GlLife = "live" | "lost" | "dead";

export const GL_RESTORE_MS = 2000;

export function glOnLost(life: GlLife): { life: GlLife; waitMs: number } {
  if (life !== "live") {
    return { life, waitMs: 0 };
  }
  return { life: "lost", waitMs: GL_RESTORE_MS };
}

export function glOnRestored(life: GlLife): GlLife {
  return life === "lost" ? "live" : life;
}

export function glOnRestoreTimeout(life: GlLife): GlLife {
  return life === "lost" ? "dead" : life;
}

export function glShouldDraw(life: GlLife): boolean {
  return life === "live";
}
