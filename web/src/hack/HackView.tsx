import Editor, { type OnMount } from "@monaco-editor/react";
import { useEffect, useMemo, useRef, useState } from "react";
import type { editor } from "monaco-editor";
import "./monacoEnv";
import { getNativeFunction, hasJuceBridge } from "../bridge/juce";
import { terminalActions } from "../app/workspace";
import { irSlotsFromScript, mergeIrSlots } from "../presets/irSlots";
import { useAstStore } from "../store/astStore";
import { useHostStore } from "../store/hostStore";
import { openImpulse } from "../overlays/ImpulsePanel";
import { useTheme } from "../theme/themeBind";
import { applyPatchDraft, discardPatchDraft } from "./terminalCompile";
import {
  annotateKnobInlays,
  termFrame,
  withHeaderComments,
} from "./dslLanguage";
import { nodeAtPosition, paramAtPosition, rangeForNode, rangeForParam } from "./dslSourceMap";
import { draftBanner, publishSelection, readPatchDocument } from "../store/patchWorkspace";
import { usePatchSelectionStore } from "../store/patchSelectionStore";
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
  const editing = useAstStore((s) => s.draftEditing);
  const setEditing = useAstStore((s) => s.setDraftEditing);
  const setOverlay = useHostStore((s) => s.setOverlay);
  const knobs = useHostStore((s) => s.knobs);
  const presetName = useHostStore((s) => s.presetName);
  const presets = useHostStore((s) => s.presets);
  const focusNodeId = usePatchSelectionStore((s) => s.primaryId);
  const focusParam = usePatchSelectionStore((s) => s.param);
  const modelRef = useRef<editor.ITextModel | null>(null);
  const monacoRef = useRef<typeof import("monaco-editor") | null>(null);
  const editorRef = useRef<editor.IStandaloneCodeEditor | null>(null);
  const decoRef = useRef<string[]>([]);
  const [editorReady, setEditorReady] = useState(0);
  const frame = termFrame(editing);
  const theme = useTheme();
  const banner = draftBanner(readPatchDocument().draftStatus);

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
    editorRef.current = ed;
    decoRef.current = [];
    setEditorReady((n) => n + 1);
    ed.onDidChangeCursorPosition((e) => {
      const text = ed.getModel()?.getValue() ?? "";
      const id = nodeAtPosition(text, e.position.lineNumber, e.position.column);
      if (! id) {
        return;
      }
      const param = paramAtPosition(text, e.position.lineNumber, e.position.column)?.param ?? null;
      publishSelection({
        nodeIds: [id],
        primaryId: id,
        param,
        origin: "terminal",
        intent: "mark",
      });
    });
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

  useEffect(() => {
    const ed = editorRef.current;
    const monaco = monacoRef.current;
    if (! ed || ! monaco) {
      return;
    }
    const text = ed.getModel()?.getValue() ?? shown;
    const range = focusNodeId && focusParam
      ? rangeForParam(text, focusNodeId, focusParam)
      : focusNodeId
        ? rangeForNode(text, focusNodeId)
        : null;
    decoRef.current = ed.deltaDecorations(
      decoRef.current,
      range
        ? [{
            range: new monaco.Range(range.startLine, 1, range.endLine, range.endColumn),
            options: {
              className: "nk-term-focus",
              isWholeLine: true,
              stickiness: monaco.editor.TrackedRangeStickiness.NeverGrowsWhenTypingAtEdges,
            },
          }]
        : [],
    );
  }, [focusNodeId, focusParam, shown, editorReady]);

  const save = async () => {
    await applyPatchDraft();
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
                onClick={() => { if (editing) void save(); else setEditing(true); }}
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
            onClick={() => discardPatchDraft()}
          >
            Cancel
          </button>
        )}
        {banner ? (
          <span className="nk-clip nk-term-tool" data-draft-banner="">
            {banner}
          </span>
        ) : null}
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
        <div className="nk-term-glow pointer-events-none" aria-hidden />
        <div className="nk-term-scan pointer-events-none" aria-hidden />
        <Editor
          height="100%"
          language={DSL_LANGUAGE_ID}
          theme={DSL_THEME}
          loading={null}
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
            lineNumbers: frame.lineNumbers,
            glyphMargin: false,
            folding: false,
            lineDecorationsWidth: frame.mode === "edit" ? 10 : 0,
            lineNumbersMinChars: frame.mode === "edit" ? 3 : 2,
            padding: { top: 10, bottom: 10 },
            scrollBeyondLastLine: false,
            wordWrap: "on",
            tabSize: 2,
            automaticLayout: true,
            snippetSuggestions: "top",
            tabCompletion: "on",
            suggest: { snippetsPreventQuickSuggestions: false, showSnippets: true },
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

export default HackView;
