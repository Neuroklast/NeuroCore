import {useTelemetryStore} from "../store/telemetryStore";
import {handoffPlan} from "./workspaceHandoff";
import {WorkspaceHandoff,type HandoffVisual} from "./handoffStreak";
import { lazy, Suspense, useCallback, useEffect, useRef, useState } from "react";
import { getNativeFunction, hasJuceBridge, onNativeEvent } from "../bridge/juce";
import type { AstEventPayload, CompileResultPayload, Origin } from "../bridge/ast";
import { shouldHydrate } from "../bridge/ast";
import { AssembleView } from "../assemble/AssembleView";
import { BindCables, BindDragGhost } from "../assemble/BindCables";

import { Footer, Hud, Knobs, MixOs, Toolbar, WorkspaceTabs } from "../chrome/Chrome";
import { TooltipLayer } from "../chrome/TooltipLayer";
import { browserShortcutStopsPropagation, canForwardHostKey, circuitHasOverlay, circuitHasSelection, shouldBlockBrowserShortcut, shouldBlockNativeContextMenu, shouldBlockWheelZoom, shouldForwardToHost } from "../chrome/shortcuts";
import { StandaloneTransportBar } from "../chrome/StandaloneTransportBar";
import { isRedoKey, isUndoKey, undoTargetIsText } from "../chrome/undoModel";
import { redoCircuit, undoCircuit } from "../assemble/addBlock";
import { Overlays } from "../overlays/Overlays";
import { TelemetryPump } from "../viz/ScopeDeck";
import { useAstStore } from "../store/astStore";

import { useHostStore } from "../store/hostStore";

import { CrtFx, PaneVignette } from "../theme/CrtFx";
import { ScaleShell } from "../theme/ScaleShell";
import { FaceView } from "../face/FaceView";
import { bindDocumentMotion, bindDocumentTheme } from "../theme/themeBind";
import { setVizFpsCap } from "../theme/vizClock";
import { syncPerfFlag } from "../perf/marks";
import { PerfHud } from "../perf/PerfHud";
import { seedFactoryPresets } from "../presets/presetActions";
import { resetMuteSolo } from "../assemble/muteSoloApply";
import { useChipViewStore } from "../store/expandStore";
import { dismissSplash, setSplashProgress } from "../theme/splash";
import { nk } from "../theme/tokens";
import { knobBindEnabled, telemetryIntervalMs, terminalMounted, workspacePaneClass, type Workspace } from "./workspace";

const HackView = lazy(() => import("../hack/HackView"));

export function App() {
  const [workspace, setWorkspace] = useState<Workspace>("face");
  const [handoff,setHandoff]=useState<HandoffVisual|null>(null);
  const frameRef=useRef<HTMLDivElement>(null);
  const finishHandoff=useCallback(()=>setHandoff(null),[]);
  const changeWorkspace=(next:Workspace)=>{
    const plan=handoffPlan(workspace,next,useHostStore.getState().motion,
      window.matchMedia("(prefers-reduced-motion: reduce)").matches,useTelemetryStore.getState(),performance.now());
    const frame=frameRef.current?.getBoundingClientRect();
    const contact=document.querySelector(plan?.direction==="out"?'[data-terminal="out"] [data-port-id]':'[data-terminal="in"] [data-port-id]')?.getBoundingClientRect();
    if(plan&&frame&&contact&&frame.width>0&&frame.height>0) {
      const scaleX=frameRef.current!.clientWidth/frame.width,scaleY=frameRef.current!.clientHeight/frame.height;
      const x=(contact.x+contact.width*.5-frame.x)*scaleX,y=(contact.y+contact.height*.5-frame.y)*scaleY;
      setHandoff(x>=0&&y>=0&&x<=frameRef.current!.clientWidth&&y<=frameRef.current!.clientHeight?
        {id:performance.now(),direction:plan.direction,opacity:plan.opacity,x,y,w:frameRef.current!.clientWidth,h:frameRef.current!.clientHeight}:null);
    } else setHandoff(null);
    setWorkspace(next);
  };
  const telemetryPath = useHostStore((s) => s.telemetryPath);
  const frameRate = useHostStore((s) => s.frameRate);
  const theme = useHostStore((s) => s.theme) ?? "signal";
  const motion = useHostStore((s) => s.motion);
  useEffect(() => {
    bindDocumentTheme(theme);
  }, [theme]);
  useEffect(() => {
    bindDocumentMotion(motion);
  }, [motion]);
  useEffect(() => {
    setVizFpsCap(frameRate);
  }, [frameRate]);
  useEffect(() => {
    syncPerfFlag();
  }, []);
  useEffect(() => {
    const w = window as Window & { __nkSetPreset?: (name: string, dirty?: boolean) => void };
    w.__nkSetPreset = (name, dirty) => {
      useHostStore.setState({ presetName: name, presetDirty: dirty === true });
    };
    return () => {
      delete w.__nkSetPreset;
    };
  }, []);
  useEffect(() => {
    const subscriptions: Array<() => void> = [];
    const listen = (id: string, fn: (payload: unknown) => void) => {
      subscriptions.push(onNativeEvent(id, fn));
    };
    listen("ast", (payload) => {
      const rec = payload as AstEventPayload;
      if (! rec || typeof rec.astJson !== "string") {
        return;
      }
      if (rec.origin === "preset") {
        resetMuteSolo();
        useChipViewStore.getState().collapseAll();
      }
      useAstStore.getState().applyAstEvent(rec, {
        updateScript: shouldHydrate("editor", rec.origin as Origin),
      });
    });
    listen("compileResult", (payload) => {
      const rec = payload as CompileResultPayload;
      if (! rec || typeof rec.ok !== "boolean") {
        return;
      }
      useAstStore.getState().applyCompileResult(rec);
    });
    listen("hello", (payload) => {
      const rec = payload as { telemetryPath?: string };
      if (typeof rec?.telemetryPath === "string") {
        useHostStore.getState().setTelemetryPath(rec.telemetryPath);
      }
    });
    listen("params", (p) => useHostStore.getState().applyParams(p as Record<string, unknown>));
    listen("host", (p) => useHostStore.getState().applyHost(p as Record<string, unknown>));
    listen("presetState", (p) => useHostStore.getState().applyPresets(p as Record<string, unknown>));
    listen("presetImportOffer", (payload) => {
      const rec = payload as { paths?: unknown };
      const paths = Array.isArray(rec?.paths) ? rec.paths.map(String).filter(Boolean) : [];
      if (paths.length > 0) {
        useHostStore.getState().offerImport(paths);
      }
    });
    listen("license", (p) => useHostStore.getState().applyLicense(p as Record<string, unknown>));
    listen("ir", (p) => useHostStore.getState().applyIr(p as Record<string, unknown>));
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
        overlayOpen: useHostStore.getState().overlay != null || circuitHasOverlay(),
      };
      // The standalone application owns Space. VST3/AU must continue forwarding
      // it to the DAW. Never intercept typing in text fields or active overlays.
      if (useHostStore.getState().standalone && !hostCtx.textTarget && !hostCtx.overlayOpen
          && !(e.target instanceof HTMLElement && e.target.closest("button, select, [role=button]"))
          && !e.repeat && !e.ctrlKey && !e.altKey && !e.metaKey && !e.shiftKey
          && (e.code === "Space" || e.key === " " || e.key === "Spacebar")) {
        e.preventDefault();
        e.stopPropagation();
        if (hasJuceBridge()) {
          void getNativeFunction("standaloneTransport")({ action: "toggle" }).catch(() => undefined);
        }
        return;
      }
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
      if (shouldBlockBrowserShortcut(e, hostCtx)) {
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

    setSplashProgress(70);
    if (hasJuceBridge()) {
      void getNativeFunction("UI_READY")({ build: nk.version })
        .catch(() => undefined)
        .finally(() => {
          setSplashProgress(100);
          dismissSplash();
        });
    } else {
      if (useAstStore.getState().ast == null) {
        seedFactoryPresets();
      }
      setSplashProgress(100);
      dismissSplash();
    }
    return () => {
      subscriptions.forEach(unsubscribe => unsubscribe());
      window.removeEventListener("keydown", onKey, true);
      window.removeEventListener("wheel", onWheel, true);
      window.removeEventListener("contextmenu", onContextMenu, true);
    };
  }, []);

  return (
    <ScaleShell>
    <main data-ws={workspace} className="nk-os relative flex h-full w-full min-h-0 min-w-0 flex-col overflow-hidden font-mono text-ink">
      <Hud />
      <Toolbar />
      <StandaloneTransportBar />
      <WorkspaceTabs workspace={workspace} setWorkspace={changeWorkspace} />
        <div className="nk-bind-host nk-chrome-x relative flex min-h-0 min-w-0 flex-1 flex-col overflow-hidden">
        <div ref={frameRef} className="nk-frame relative min-h-0 flex-1 overflow-hidden border border-[var(--nk-line)]">
          <div className="nk-panes">
          {workspace === "face" ? (
            <div className={workspacePaneClass(true)} data-pane="face">
              <FaceView />
            </div>
          ) : null}
          <div className={workspacePaneClass(workspace === "assemble")} data-pane="assemble">
            <AssembleView active={workspace === "assemble"} onOpenTerminal={() => changeWorkspace("hack")} />
          </div>
          {terminalMounted(workspace) ? (
            <div className={workspacePaneClass(true)} data-pane="hack">
              <Suspense fallback={null}>
                <HackView />
              </Suspense>
            </div>
          ) : null}
          </div>
          {handoff ? <WorkspaceHandoff key={handoff.id} visual={handoff} onFinish={finishHandoff} /> : null}
          <PaneVignette />
        </div>
        <MixOs />
        <Knobs bind={knobBindEnabled(workspace)} rail="bottom" />
        {workspace === "assemble" ? <BindCables /> : null}
        <BindDragGhost />
      </div>
      <TelemetryPump telemetryPath={telemetryPath} intervalMs={telemetryIntervalMs(workspace, frameRate)} />
      <Footer />
      <CrtFx />
      <Overlays />
      <TooltipLayer />
      <PerfHud />
    </main>
    </ScaleShell>
  );
}
