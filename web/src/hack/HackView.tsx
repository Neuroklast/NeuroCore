import Editor, { type OnMount } from "@monaco-editor/react";
import { useEffect, useMemo, useRef, useState } from "react";
import type { editor } from "monaco-editor";
import { publishScript } from "../assemble/addBlock";
import { getNativeFunction, hasJuceBridge } from "../bridge/juce";
import { terminalActions } from "../app/workspace";
import { validateOnSave } from "../overlays/validateModel";
import { irSlotsFromScript, mergeIrSlots } from "../presets/irSlots";
import { useAstStore } from "../store/astStore";
import { useHostStore } from "../store/hostStore";
import { openImpulse } from "../overlays/ImpulsePanel";
import { useTheme } from "../theme/themeBind";
import {
  annotateKnobInlays,
  termFrame,
  withHeaderComments,
} from "./dslLanguage";
import {
  defineDslTheme,
  DSL_LANGUAGE_ID,
  DSL_THEME,
  registerNeurokoreDsl,
} from "./registerDsl";

const LINT_MS = 250;

export function HackView() {
  const script = useAstStore((s) => s.script);
  const lastValidScript = useAstStore((s) => s.lastValidScript);
  const diagnostics = useAstStore((s) => s.diagnostics);
  const setDraftScript = useAstStore((s) => s.setDraftScript);
  const [editing, setEditing] = useState(false);
  const setOverlay = useHostStore((s) => s.setOverlay);
  const knobs = useHostStore((s) => s.knobs);
  const presetName = useHostStore((s) => s.presetName);
  const presets = useHostStore((s) => s.presets);
  const modelRef = useRef<editor.ITextModel | null>(null);
  const monacoRef = useRef<typeof import("monaco-editor") | null>(null);
  const frame = termFrame(editing);
  const theme = useTheme();

  useEffect(() => {
    const monaco = monacoRef.current;
    const model = modelRef.current;
    if (! monaco || ! model) {
      return;
    }
    monaco.editor.setModelMarkers(
      model,
      "neurokore",
      diagnostics.map((d) => ({
        startLineNumber: Math.max(1, d.line),
        startColumn: Math.max(1, d.column),
        endLineNumber: Math.max(1, d.line),
        endColumn: 200,
        message: d.message,
        severity: monaco.MarkerSeverity.Error,
      })),
    );
  }, [diagnostics]);

  useEffect(() => {
    if (! editing || ! hasJuceBridge()) {
      return;
    }
    const handle = window.setTimeout(() => {
      void getNativeFunction("lint")({ script }).catch(() => undefined);
    }, LINT_MS);
    return () => window.clearTimeout(handle);
  }, [script, editing]);

  useEffect(() => {
    const monaco = monacoRef.current;
    if (! monaco) {
      return;
    }
    defineDslTheme(monaco, theme);
    monaco.editor.setTheme(DSL_THEME);
  }, [theme]);

  const onMount: OnMount = (ed, monaco) => {
    registerNeurokoreDsl(monaco);
    defineDslTheme(monaco, theme);
    monaco.editor.setTheme(DSL_THEME);
    modelRef.current = ed.getModel();
    monacoRef.current = monaco;
  };

  const howItSounds = useMemo(() => {
    const hit = presets.find((p) => p.name === presetName);
    return hit?.description ?? "";
  }, [presets, presetName]);

  const baseScript = editing ? script : (lastValidScript || script);
  const shown = useMemo(() => {
    if (editing) {
      return baseScript;
    }
    const headed = withHeaderComments(baseScript, presetName || "Untitled", howItSounds);
    return annotateKnobInlays(headed, knobs);
  }, [editing, baseScript, presetName, howItSounds, knobs]);

  const save = () => {
    validateOnSave(script, diagnostics);
    publishScript(script, "editor");
    setEditing(false);
  };

  const formulaPt = useHostStore((s) => s.formulaPt);
  const hostIr = useHostStore((s) => s.irSlots);
  const cabRows = useMemo(
    () => mergeIrSlots(irSlotsFromScript(baseScript), hostIr),
    [baseScript, hostIr],
  );

  return (
    <section className="flex h-full min-h-0 flex-1 flex-col bg-black">
      <div className="nk-term-bar flex shrink-0 items-center gap-2 border-b border-panel">
        {terminalActions().map((action) => {
          if (action === "edit") {
            return (
              <button
                key={action}
                type="button"
                className="nk-clip nk-term-tool"
                onClick={() => (editing ? save() : setEditing(true))}
              >
                {editing ? "Save" : "Edit"}
              </button>
            );
          }
          return (
            <button
              key={action}
              type="button"
              className="nk-clip nk-term-tool"
              onClick={() => setOverlay(action)}
            >
              {action === "validate" ? "Validate" : "Optimize"}
            </button>
          );
        })}
        {editing && (
          <button
            type="button"
            className="nk-clip nk-term-tool"
            onClick={() => {
              setDraftScript(lastValidScript);
              setEditing(false);
            }}
          >
            Cancel
          </button>
        )}
        {cabRows.map((row) => (
          <button
            key={row.slot}
            type="button"
            className="nk-clip nk-term-tool"
            title={row.loaded ? row.name : `Load IR for ${row.slot}`}
            onClick={() => openImpulse(row.slot)}
          >
            {row.slot} {row.loaded ? row.name : "IR"}
          </button>
        ))}
      </div>
      <div
        className={`nk-term relative min-h-0 flex-1 overflow-hidden ${
          frame.frame === "accent" ? "nk-term--edit" : "nk-term--view"
        }`}
        data-mode={frame.mode}
      >
        <div className="nk-term-scan pointer-events-none" aria-hidden />
        <Editor
          height="100%"
          language={DSL_LANGUAGE_ID}
          theme={DSL_THEME}
          value={shown}
          onChange={(v) => {
            if (editing) {
              setDraftScript(v ?? "");
            }
          }}
          onMount={onMount}
          options={{
            readOnly: frame.readOnly,
            minimap: { enabled: false },
            fontFamily: "JetBrains Mono, ui-monospace, Consolas, monospace",
            fontSize: formulaPt,
            lineNumbers: "on",
            scrollBeyondLastLine: false,
            wordWrap: "on",
            tabSize: 2,
            automaticLayout: true,
            cursorStyle: "line",
            cursorWidth: frame.caret ? 2 : 0,
            renderLineHighlight: frame.caret ? "line" : "none",
            overviewRulerLanes: 0,
          }}
        />
      </div>
    </section>
  );
}
