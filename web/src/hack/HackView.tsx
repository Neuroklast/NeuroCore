import Editor, { type OnMount } from "@monaco-editor/react";
import { useEffect, useRef, useState } from "react";
import type { editor } from "monaco-editor";
import { publishScript } from "../assemble/addBlock";
import { getNativeFunction, hasJuceBridge } from "../bridge/juce";
import { terminalActions } from "../app/workspace";
import { useAstStore } from "../store/astStore";
import { useHostStore } from "../store/hostStore";
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
  const modelRef = useRef<editor.ITextModel | null>(null);
  const monacoRef = useRef<typeof import("monaco-editor") | null>(null);

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

  const onMount: OnMount = (ed, monaco) => {
    registerNeurokoreDsl(monaco);
    defineDslTheme(monaco);
    monaco.editor.setTheme(DSL_THEME);
    modelRef.current = ed.getModel();
    monacoRef.current = monaco;
  };

  const shown = editing ? script : (lastValidScript || script);

  const save = () => {
    publishScript(script, "editor");
    setEditing(false);
  };

  const formulaPt = useHostStore((s) => s.formulaPt);

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
      </div>
      <div className="nk-term relative min-h-0 flex-1 overflow-hidden">
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
            readOnly: ! editing,
            minimap: { enabled: false },
            fontFamily: "JetBrains Mono, ui-monospace, Consolas, monospace",
            fontSize: formulaPt,
            lineNumbers: "on",
            scrollBeyondLastLine: false,
            wordWrap: "on",
            tabSize: 2,
            automaticLayout: true,
          }}
        />
      </div>
    </section>
  );
}
