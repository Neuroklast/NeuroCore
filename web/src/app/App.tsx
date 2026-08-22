import { useEffect, useState } from "react";
import { getNativeFunction, hasJuceBridge, onNativeEvent } from "../bridge/juce";
import type { AstEventPayload, CompileResultPayload, Origin } from "../bridge/ast";
import { shouldHydrate } from "../bridge/ast";
import { AssembleView } from "../assemble/AssembleView";
import { BindCables, BindDragGhost } from "../assemble/BindCables";

import { Footer, Hud, Knobs, MixOs, Toolbar, WorkspaceTabs } from "../chrome/Chrome";
import { browserShortcutStopsPropagation, canForwardHostKey, circuitHasSelection, shouldBlockBrowserShortcut, shouldBlockNativeContextMenu, shouldBlockWheelZoom, shouldForwardToHost } from "../chrome/shortcuts";
import { isRedoKey, isUndoKey, undoTargetIsText } from "../chrome/undoModel";
import { redoCircuit, undoCircuit } from "../assemble/addBlock";
import { HackView } from "../hack/HackView";
import "../hack/monacoEnv";
import { Overlays } from "../overlays/Overlays";
import { TelemetryPump } from "../viz/ScopeDeck";
import { useAstStore } from "../store/astStore";

import { useHostStore } from "../store/hostStore";

import { RESIZE_GRIP } from "../theme/chromeSpec";
import { CrtFx, PaneTechNoise, PaneVignette } from "../theme/CrtFx";
import { ScaleShell } from "../theme/ScaleShell";
import { FaceView } from "../face/FaceView";
import { bindDocumentTheme } from "../theme/themeBind";
import { setVizFpsCap } from "../theme/vizClock";
import { seedFactoryPresets } from "../presets/presetActions";
import { parseDslSketch } from "../presets/parseDslSketch";
import { stripMuteComments } from "../assemble/muteSolo";
import { resetMuteSolo } from "../assemble/muteSoloApply";
import { useChipViewStore } from "../store/expandStore";
import { knobBindEnabled, telemetryIntervalMs, type Workspace } from "./workspace";

export function App() {
  const [workspace, setWorkspace] = useState<Workspace>("face");
  const telemetryPath = useHostStore((s) => s.telemetryPath);
  const frameRate = useHostStore((s) => s.frameRate);
  const theme = useHostStore((s) => s.theme) ?? "signal";
  useEffect(() => {
    bindDocumentTheme(theme);
    requestAnimationFrame(() => {
      document.documentElement.dataset.nkReady = "1";
      document.getElementById("nk-splash")?.remove();
    });
  }, [theme]);
  useEffect(() => {
    setVizFpsCap(frameRate);
  }, [frameRate]);
  useEffect(() => {
    onNativeEvent("ast", (payload) => {
      const rec = payload as AstEventPayload;
      if (! rec || typeof rec.astJson !== "string") {
        return;
      }
      if (rec.origin === "preset" || rec.origin === "host") {
        resetMuteSolo();
        useChipViewStore.getState().collapseAll();
      }
      const clean = typeof rec.script === "string" ? stripMuteComments(rec.script) : rec.script;
      const astJson = clean && clean !== rec.script
        ? JSON.stringify(parseDslSketch(clean).doc)
        : rec.astJson;
      useAstStore.getState().applyAstEvent({ ...rec, astJson }, {
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
      const hostCtx = {
        textTarget: undoTargetIsText(e.target),
        circuitHasSelection: circuitHasSelection(),
        overlayOpen: useHostStore.getState().overlay != null,
      };
      if (shouldForwardToHost(e, hostCtx)) {
        // Only swallow keys native can map — otherwise plugin and Cubase both lose them.
        if (! canForwardHostKey(e))
          return;
        e.preventDefault();
        e.stopPropagation();
        if (hasJuceBridge()) {
          void getNativeFunction("hostKey")({
            key: e.key,
            code: e.code,
            ctrl: e.ctrlKey,
            alt: e.altKey,
            shift: e.shiftKey,
            meta: e.metaKey,
          }).catch(() => undefined);
        }
        return;
      }
      // Issue 2: Ctrl+A inside the DSL code editor (Monaco / any text input) must
      // reach the editor's native select-all handler.  undoTargetIsText already
      // detects Monaco containers via the `.monaco-editor` class selector.
      if ((e.ctrlKey || e.metaKey) && e.key.toLowerCase() === "a" && undoTargetIsText(e.target)) {
        return;
      }
      if (shouldBlockBrowserShortcut(e)) {
        e.preventDefault();
        // Ctrl/Cmd+A Arrange: block browser select-all but let AssembleView bubble-handle.
        if (browserShortcutStopsPropagation(e, { textTarget: hostCtx.textTarget }))
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
      void getNativeFunction("UI_READY")({ build: "0.6.0-beta", scale: 1 }).catch(() => undefined);
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
    <main data-ws={workspace} className="nk-os relative flex h-[860px] w-[1280px] flex-col overflow-hidden font-mono text-ink">
      <Hud />
      <Toolbar />
      <div className="flex h-[28px] shrink-0 items-stretch">
        <div className="min-w-0 flex-1">
          <WorkspaceTabs workspace={workspace} setWorkspace={setWorkspace} />
        </div>
      </div>
      <div className="nk-bind-host relative flex min-h-0 min-w-0 flex-1 flex-col overflow-hidden">
        <div className="nk-frame relative min-h-0 flex-1 overflow-hidden border border-[var(--nk-line)]">
          {workspace === "face" ? (
            <div className="h-full min-h-0">
              <FaceView />
            </div>
          ) : null}
          <div className={workspace === "assemble" ? "h-full min-h-0" : "hidden"}>
            <AssembleView />
          </div>
          <div className={workspace === "hack" ? "h-full min-h-0" : "hidden"}>
            <HackView />
          </div>
          <PaneTechNoise />
          <PaneVignette />
        </div>
        <MixOs />
        <Knobs bind={knobBindEnabled(workspace)} rail="bottom" />
        {workspace === "assemble" ? <BindCables /> : null}
        <BindDragGhost />
      </div>
      <TelemetryPump telemetryPath={telemetryPath} intervalMs={telemetryIntervalMs(workspace, frameRate)} />
      <Footer />
      <Overlays />

      <CrtFx />
      <div className="nk-resize-grip" style={{ width: RESIZE_GRIP, height: RESIZE_GRIP }} aria-hidden />
    </main>
    </ScaleShell>
  );
}
