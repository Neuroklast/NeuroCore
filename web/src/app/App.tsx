import { useEffect, useState } from "react";
import { getNativeFunction, hasJuceBridge, onNativeEvent } from "../bridge/juce";
import type { AstEventPayload, CompileResultPayload, Origin } from "../bridge/ast";
import { shouldHydrate } from "../bridge/ast";
import { AssembleView } from "../assemble/AssembleView";
import { BindCables, BindDragGhost } from "../assemble/BindCables";

import { Footer, Hud, Knobs, MixOs, Toolbar, WorkspaceTabs } from "../chrome/Chrome";
import { isHostTransportKey, shouldBlockBrowserShortcut, shouldBlockNativeContextMenu, shouldBlockWheelZoom } from "../chrome/shortcuts";
import { isRedoKey, isUndoKey, undoTargetIsText } from "../chrome/undoModel";
import { redoCircuit, undoCircuit } from "../assemble/addBlock";
import { HackView } from "../hack/HackView";
import "../hack/monacoEnv";
import { Overlays } from "../overlays/Overlays";
import { ScopeDeck } from "../viz/ScopeDeck";
import { useAstStore } from "../store/astStore";

import { useHostStore } from "../store/hostStore";
import { shouldPlayBoot } from "../theme/boot";
import { RESIZE_GRIP } from "../theme/chromeSpec";
import { CrtFx, PaneVignette } from "../theme/CrtFx";
import { ScaleShell } from "../theme/ScaleShell";
import { FaceView } from "../face/FaceView";
import { bindDocumentTheme } from "../theme/themeBind";
import { seedFactoryPresets } from "../presets/presetActions";
import { knobBindEnabled, knobRail, type Workspace } from "./workspace";

export function App() {
  const [workspace, setWorkspace] = useState<Workspace>("face");
  const telemetryPath = useHostStore((s) => s.telemetryPath);
  const motion = useHostStore((s) => s.motion);
  const reduced = typeof window !== "undefined"
    && window.matchMedia?.("(prefers-reduced-motion: reduce)").matches === true;
  const boot = shouldPlayBoot(reduced, motion);
  const theme = useHostStore((s) => s.theme) ?? "signal";
  useEffect(() => {
    bindDocumentTheme(theme);
  }, [theme]);
  useEffect(() => {
    onNativeEvent("ast", (payload) => {
      const rec = payload as AstEventPayload;
      if (! rec || typeof rec.astJson !== "string") {
        return;
      }
      useAstStore.getState().applyAstEvent(rec, {
        updateScript: shouldHydrate("editor", rec.origin as Origin),
      });
    });
    onNativeEvent("compileResult", (payload) => {
      const rec = payload as CompileResultPayload;
      if (! rec || typeof rec.ok !== "boolean") {
        return;
      }
      useAstStore.getState().applyCompileResult(rec);
    });
    onNativeEvent("hello", (payload) => {
      const rec = payload as { telemetryPath?: string };
      if (typeof rec?.telemetryPath === "string") {
        useHostStore.getState().setTelemetryPath(rec.telemetryPath);
      }
    });
    onNativeEvent("params", (p) => useHostStore.getState().applyParams(p as Record<string, unknown>));
    onNativeEvent("host", (p) => useHostStore.getState().applyHost(p as Record<string, unknown>));
    onNativeEvent("presetState", (p) => useHostStore.getState().applyPresets(p as Record<string, unknown>));
    onNativeEvent("license", (p) => useHostStore.getState().applyLicense(p as Record<string, unknown>));
    onNativeEvent("ir", (p) => useHostStore.getState().applyIr(p as Record<string, unknown>));
    const onKey = (e: KeyboardEvent) => {
      if (! undoTargetIsText(e.target) && (isUndoKey(e) || isRedoKey(e))) {
        e.preventDefault();
        e.stopPropagation();
        void (isUndoKey(e) ? undoCircuit() : redoCircuit());
        return;
      }
      if (isHostTransportKey(e) && ! undoTargetIsText(e.target)) {
        e.preventDefault();
        e.stopPropagation();
        if (hasJuceBridge())
          void getNativeFunction("hostKey")({ key: "Space" }).catch(() => undefined);
        return;
      }
      if (shouldBlockBrowserShortcut(e)) {
        e.preventDefault();
        e.stopPropagation();
      }
    };
    const onWheel = (e: WheelEvent) => {
      if (shouldBlockWheelZoom(e)) {
        e.preventDefault();
      }
    };
    const onContextMenu = (e: Event) => {
      if (shouldBlockNativeContextMenu({ target: e.target })) {
        e.preventDefault();
      }
    };
    window.addEventListener("keydown", onKey, true);
    window.addEventListener("wheel", onWheel, { capture: true, passive: false });
    window.addEventListener("contextmenu", onContextMenu, true);

    if (hasJuceBridge()) {
      void getNativeFunction("UI_READY")({ build: "0.4.8-alpha", scale: 1 }).catch(() => undefined);
    } else if (useAstStore.getState().ast == null) {
      seedFactoryPresets();
    }
    return () => {
      window.removeEventListener("keydown", onKey, true);
      window.removeEventListener("wheel", onWheel, true);
      window.removeEventListener("contextmenu", onContextMenu, true);
    };
  }, []);

  return (
    <ScaleShell>
    <main data-ws={workspace} className={`nk-os relative flex h-[860px] w-[1280px] flex-col overflow-hidden font-mono text-ink ${boot ? "nk-boot" : ""}`}>
      <Hud />
      <Toolbar />
      <div className="flex h-[28px] shrink-0 items-stretch">
        <div className="min-w-0 flex-1">
          <WorkspaceTabs workspace={workspace} setWorkspace={setWorkspace} />
        </div>
      </div>
      <div className="flex min-h-0 flex-1 overflow-hidden">
        {knobRail(workspace) === "left" ? (
          <aside className="flex w-[148px] shrink-0 flex-col overflow-hidden border-r border-panel">
            <Knobs bind={knobBindEnabled(workspace)} rail="left" />
          </aside>
        ) : null}
        <div className="nk-bind-host relative flex min-h-0 min-w-0 flex-1 flex-col overflow-hidden">
          <div className="nk-frame relative min-h-0 flex-1 overflow-hidden border border-accent/35">
            {workspace === "face" && <FaceView open={setWorkspace} />}
            <div className={workspace === "assemble" ? "h-full min-h-0" : "hidden"}>
              <AssembleView />
            </div>
            {workspace === "hack" && <HackView />}
            <PaneVignette />
          </div>
          {knobRail(workspace) === "bottom" ? (
            <Knobs bind={knobBindEnabled(workspace)} rail="bottom" />
          ) : null}
          {workspace === "assemble" ? <BindCables /> : null}
          <BindDragGhost />
          <MixOs />
        </div>
      </div>
      <ScopeDeck telemetryPath={telemetryPath} />
      <Footer />
      <Overlays />

      <CrtFx />
      <div className="nk-resize-grip" style={{ width: RESIZE_GRIP, height: RESIZE_GRIP }} aria-hidden />
    </main>
    </ScaleShell>
  );
}
