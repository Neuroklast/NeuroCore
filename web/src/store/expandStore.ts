import { create } from "zustand";

/** Name (title + jacks) or detail (all live values). No accordion. */
export const useChipViewStore = create<{
  detail: Record<string, boolean>;
  isDetail: (id: string) => boolean;
  toggle: (id: string) => void;
  setDetail: (id: string, on: boolean) => void;
}>((set, get) => ({
  detail: {},
  isDetail: (id) => Boolean(get().detail[id]),
  toggle: (id) => set((s) => ({ detail: { ...s.detail, [id]: ! s.detail[id] } })),
  setDetail: (id, on) => set((s) => ({ detail: { ...s.detail, [id]: on } })),
}));


