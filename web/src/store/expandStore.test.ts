import { describe, expect, it, beforeEach } from "vitest";
import { shouldCollapseChipDetail, useChipViewStore } from "./expandStore";

describe("expandStore mute/solo", () => {
  beforeEach(() => {
    // Reset the store before each test
    useChipViewStore.setState({ 
      detail: {},
      muted: new Set(),
      soloed: new Set()
    });
  });

  it("mutes a block", () => {
    const store = useChipViewStore.getState();
    expect(store.isMuted("block1")).toBe(false);
    
    store.toggleMute("block1");
    expect(store.isMuted("block1")).toBe(true);
    expect(store.isAudible("block1")).toBe(false);
    
    store.toggleMute("block1");
    expect(store.isMuted("block1")).toBe(false);
    expect(store.isAudible("block1")).toBe(true);
  });

  it("solos a block", () => {
    const store = useChipViewStore.getState();
    expect(store.isSoloed("block1")).toBe(false);
    
    store.toggleSolo("block1");
    expect(store.isSoloed("block1")).toBe(true);
    expect(store.isAudible("block1")).toBe(true);
    
    store.toggleSolo("block1");
    expect(store.isSoloed("block1")).toBe(false);
  });

  it("solo takes precedence over mute", () => {
    const store = useChipViewStore.getState();
    
    // Mute block1
    store.toggleMute("block1");
    expect(store.isAudible("block1")).toBe(false);
    
    // Solo block1 (should override mute)
    store.toggleSolo("block1");
    expect(store.isAudible("block1")).toBe(true);
  });

  it("when any blocks are soloed, only soloed blocks are audible", () => {
    const store = useChipViewStore.getState();
    
    // Solo block1
    store.toggleSolo("block1");
    
    // block1 is audible, block2 is not
    expect(store.isAudible("block1")).toBe(true);
    expect(store.isAudible("block2")).toBe(false);
    
    // Solo block2 as well
    store.toggleSolo("block2");
    
    // Both are audible
    expect(store.isAudible("block1")).toBe(true);
    expect(store.isAudible("block2")).toBe(true);
    
    // block3 is not soloed, so not audible
    expect(store.isAudible("block3")).toBe(false);
  });

  it("clearSolo removes all solos", () => {
    const store = useChipViewStore.getState();
    
    store.toggleSolo("block1");
    store.toggleSolo("block2");
    expect(useChipViewStore.getState().soloed.size).toBe(2);
    
    store.clearSolo();
    expect(useChipViewStore.getState().soloed.size).toBe(0);
    expect(useChipViewStore.getState().isAudible("block1")).toBe(true);
    expect(useChipViewStore.getState().isAudible("block2")).toBe(true);
  });

  it("when no solos, muted blocks are inaudible", () => {
    const store = useChipViewStore.getState();
    
    store.toggleMute("block1");
    expect(store.isAudible("block1")).toBe(false);
    expect(store.isAudible("block2")).toBe(true);
    
    store.toggleMute("block2");
    expect(store.isAudible("block1")).toBe(false);
    expect(store.isAudible("block2")).toBe(false);
  });

  it("collapses every open chip on an empty-board click, not during bind drag", () => {
    const store = useChipViewStore.getState();
    store.setDetail("filter1", true);
    store.setDetail("eq1", true);
    store.collapseAll();
    expect(useChipViewStore.getState().isDetail("filter1")).toBe(false);
    expect(useChipViewStore.getState().isDetail("eq1")).toBe(false);

    const pane = { closest: () => null };
    const sock = { closest: (sel: string) => (sel.includes("data-chip-keep-open") ? sock : null) };
    const knob = { closest: (sel: string) => (sel.includes("data-knob-bind") ? knob : null) };
    expect(shouldCollapseChipDetail(pane, null)).toBe(true);
    expect(shouldCollapseChipDetail(sock, null)).toBe(false);
    expect(shouldCollapseChipDetail(knob, null)).toBe(false);
    expect(shouldCollapseChipDetail(pane, "a")).toBe(false);
  });
});
