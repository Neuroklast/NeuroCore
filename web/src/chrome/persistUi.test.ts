import { describe, expect, it } from "vitest";
import { useHostStore } from "../store/hostStore";
import { applyUiPrefs, clampFrameRate, FRAME_RATES, persistUi } from "./persistUi";

describe("shared UI prefs", () => {
  it("takes motion, cables, theme, fps, and unsaved prompt from the host snapshot", () => {
    const next = applyUiPrefs(
      { motion: "reduced", cables: "dots", theme: "gold", frameRate: 30, discardPrompt: false },
      { motion: "full", cables: "wave", theme: "signal", frameRate: 60, discardPrompt: true, live: false, scale: 100 },
    );
    expect(next).toEqual({
      motion: "reduced",
      cables: "dots",
      theme: "gold",
      frameRate: 30,
      discardPrompt: false,
      live: false,
      scale: 100,
    });
  });

  it("takes LIVE and ui scale from the host snapshot", () => {
    const next = applyUiPrefs(
      { live: true, scale: 125, mode: "LIVE" },
      { motion: "full", cables: "dots", theme: "signal", frameRate: 60, discardPrompt: true, live: false, scale: 100 },
    );
    expect(next.live).toBe(true);
    expect(next.scale).toBe(125);
  });

  it("offers only 30 and 60 fps and maps Display/0 to 60", () => {
    expect(FRAME_RATES).toEqual([30, 60]);
    expect(clampFrameRate(0)).toBe(60);
    expect(clampFrameRate(30)).toBe(30);
    expect(clampFrameRate(60)).toBe(60);
    expect(clampFrameRate(120)).toBe(60);
    expect(applyUiPrefs({ frameRate: 0 }, { frameRate: 30 }).frameRate).toBe(60);
  });

  it("does not persist shared prefs through WebView2 localStorage", () => {
    const writes: string[] = [];
    const previous = (globalThis as { localStorage?: Storage }).localStorage;
    (globalThis as { localStorage?: Pick<Storage, "getItem" | "setItem" | "removeItem"> }).localStorage = {
      getItem: () => "gold",
      setItem: (k) => {
        writes.push(k);
      },
      removeItem: () => undefined,
    };
    persistUi({ theme: "azure", frameRate: 30, discardPrompt: false, cables: "dots" });
    expect(writes).toEqual([]);
    const s = useHostStore.getState();
    expect(s.theme).toBe("azure");
    expect(s.frameRate).toBe(30);
    expect(s.discardPrompt).toBe(false);
    expect(s.cables).toBe("dots");
    if (previous) {
      (globalThis as { localStorage?: Storage }).localStorage = previous;
    } else {
      delete (globalThis as { localStorage?: Storage }).localStorage;
    }
  });

  it("ignores missing keys so a telemetry tick cannot wipe prefs", () => {
    const cur = { motion: "off" as const, cables: "wave" as const, theme: "azure" as const, frameRate: 60 as const, discardPrompt: true, live: false, scale: 100 as const };
    expect(applyUiPrefs({ cpu: 12 }, cur)).toEqual(cur);
  });

  it("takes Unit meter prefs from the host snapshot", () => {
    const next = applyUiPrefs(
      {
        scopeSource: "out",
        scopeX: "freq",
        scopeY: "db",
        scopeGrid: false,
        scopeInvertY: true,
        scopeDelta: true,
      },
      {
        motion: "full",
        cables: "dots",
        theme: "signal",
        frameRate: 60,
        discardPrompt: true,
        live: false,
        scale: 100,
        scopeSource: "both",
        scopeX: "samples",
        scopeY: "linear",
        scopeGrid: true,
        scopeInvertY: false,
        scopeDelta: false,
      },
    );
    expect(next.scopeSource).toBe("out");
    expect(next.scopeX).toBe("freq");
    expect(next.scopeY).toBe("db");
    expect(next.scopeGrid).toBe(false);
    expect(next.scopeInvertY).toBe(true);
    expect(next.scopeDelta).toBe(true);
  });

  it("persists Unit meter prefs to the host store, not localStorage", () => {
    const writes: string[] = [];
    const previous = (globalThis as { localStorage?: Storage }).localStorage;
    (globalThis as { localStorage?: Pick<Storage, "getItem" | "setItem" | "removeItem"> }).localStorage = {
      getItem: () => "",
      setItem: (k) => {
        writes.push(k);
      },
      removeItem: () => undefined,
    };
    persistUi({
      scopeSource: "in",
      scopeX: "time",
      scopeY: "db",
      scopeGrid: false,
      scopeInvertY: true,
      scopeDelta: true,
    });
    expect(writes).toEqual([]);
    const s = useHostStore.getState();
    expect(s.scopeSource).toBe("in");
    expect(s.scopeX).toBe("time");
    expect(s.scopeY).toBe("db");
    expect(s.scopeGrid).toBe(false);
    expect(s.scopeInvertY).toBe(true);
    expect(s.scopeDelta).toBe(true);
    if (previous) {
      (globalThis as { localStorage?: Storage }).localStorage = previous;
    } else {
      delete (globalThis as { localStorage?: Storage }).localStorage;
    }
  });
});
