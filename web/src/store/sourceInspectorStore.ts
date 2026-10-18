import { create } from "zustand";
import { SOURCE_INSPECTOR } from "../assemble/sourceInspectorLayout";

type SourceInspectorState = {
  open: boolean;
  width: number;
  height: number;
  setOpen: (open: boolean) => void;
  toggle: () => void;
  setWidth: (width: number) => void;
  setHeight: (height: number) => void;
};

export const useSourceInspectorStore = create<SourceInspectorState>((set) => ({
  open: false,
  width: SOURCE_INSPECTOR.width,
  height: SOURCE_INSPECTOR.bottomHeight,
  setOpen: (open) => set({ open }),
  toggle: () => set((s) => ({ open: ! s.open })),
  setWidth: (width) => set({ width }),
  setHeight: (height) => set({ height }),
}));
