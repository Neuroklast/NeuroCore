import { create } from "zustand";

/** Name (title + jacks) or detail (all live values). No accordion. */
export const useChipViewStore = create<{
  detail: Record<string, boolean>;
  isDetail: (id: string) => boolean;
  toggle: (id: string) => void;
  setDetail: (id: string, on: boolean) => void;
  muted: Set<string>;
  soloed: Set<string>;
  toggleMute: (id: string) => void;
  toggleSolo: (id: string) => void;
  clearSolo: () => void;
  isMuted: (id: string) => boolean;
  isSoloed: (id: string) => boolean;
  isAudible: (id: string) => boolean;
}>((set, get) => ({
  detail: {},
  isDetail: (id) => Boolean(get().detail[id]),
  toggle: (id) => set((s) => ({ detail: { ...s.detail, [id]: ! s.detail[id] } })),
  setDetail: (id, on) => set((s) => ({ detail: { ...s.detail, [id]: on } })),
  muted: new Set(),
  soloed: new Set(),
  toggleMute: (id) => set((s) => {
    const next = new Set(s.muted);
    if (next.has(id)) {
      next.delete(id);
    } else {
      next.add(id);
    }
    return { muted: next };
  }),
  toggleSolo: (id) => set((s) => {
    const next = new Set(s.soloed);
    if (next.has(id)) {
      next.delete(id);
    } else {
      next.add(id);
    }
    return { soloed: next };
  }),
  clearSolo: () => set({ soloed: new Set() }),
  isMuted: (id) => get().muted.has(id),
  isSoloed: (id) => get().soloed.has(id),
  isAudible: (id) => {
    const state = get();
    // If any blocks are soloed, only soloed blocks are audible
    if (state.soloed.size > 0) {
      return state.soloed.has(id);
    }
    // Otherwise, only muted blocks are inaudible
    return ! state.muted.has(id);
  },
}));


