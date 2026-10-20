import { type OnMount } from "@monaco-editor/react";
import { useEffect, useMemo, useRef, useState } from "react";
import type { editor } from "monaco-editor";
import { PATCH_MODEL_URI } from "./patchEditorSession";
import { PatchEditor } from "./PatchEditor";
import { getNativeFunction, hasJuceBridge } from "../bridge/juce";
import { terminalActions } from "../app/workspace";
import { irSlotsFromScript, mergeIrSlots } from "../presets/irSlots";
import { useAstStore } from "../store/astStore";
import { useHostStore } from "../store/hostStore";
import { openImpulse } from "../overlays/ImpulsePanel";
import { useTheme } from "../theme/themeBind";
import { applyPatchDraft, discardPatchDraft } from "./terminalCompile";
import {
  findKnobTokens,
  termFrame,
  withHeaderComments,
} from "./dslLanguage";
import { rangeForNode, rangeForParam } from "./dslSourceMap";
import { draftBanner, publishSelection, readPatchDocument } from "../store/patchWorkspace";
import { patchStructure } from "../store/patchStructure";
import { usePatchSelectionStore } from "../store/patchSelectionStore";
import { useBindStore } from "../store/telemetryStore";
import { formatKnobDisplay } from "../chrome/noteValue";
import {
  defineDslTheme,
  DSL_THEME,
  registerNeurokoreDsl,
} from "./registerDsl";

const LINT_MS = 250;

export function HackView() {
  const script = useAstStore((s) => s.script);
  const lastValidScript = useAstStore((s) => s.lastValidScript);
  const lastValidAst = useAstStore((s) => s.lastValidAst);
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
  const structure = useMemo(
    () => (lastValidAst ? patchStructure(lastValidAst) : []),
    [lastValidAst],
  );

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
    return withHeaderComments(baseScript, presetName || "Untitled", howItSounds);
  }, [editing, baseScript, presetName, howItSounds]);
  const knobTokens = useMemo(() => findKnobTokens(shown), [shown]);
  const hover = useBindStore((s) => s.hover);
  const liveDecoRef = useRef<string[]>([]);
  const hoverDecoRef = useRef<string[]>([]);

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

  // Live Makro-Werte als stabile Inline-Decorations (keine Modellmutation,
  // kein Cursor-/Scrollsprung, kein Undo, kein Neu-Compile pro Tick).
  useEffect(() => {
    const ed = editorRef.current;
    const monaco = monacoRef.current;
    if (! ed || ! monaco) {
      return;
    }
    const model = ed.getModel();
    if (! model) {
      return;
    }
    const byLetter = new Map(knobs.map((k) => [k.id, k]));
    const liveDecos: editor.IModelDeltaDecoration[] = [];

    // Dynamisches CSS für die Inline-Werte pro Buchstabe (alles "a" zeigt den
    // aktuellen A-Wert, ohne das Modell zu berühren).
    const styleEl = document.getElementById("nk-term-live-values");
    const el = styleEl ?? (() => {
      const e = document.createElement("style");
      e.id = "nk-term-live-values";
      document.head.appendChild(e);
      return e;
    })();
    let css = "";
    for (const tok of knobTokens) {
      const k = byLetter.get(tok.letter);
      if (! k) {
        continue;
      }
      const mapped = k.min + k.value * (k.max - k.min);
      css += `.nk-term-val-${tok.letter}::after { content: "[${formatKnobDisplay(mapped, k.unit)}]"; }\n`;
      liveDecos.push({
        range: new monaco.Range(tok.line, tok.startColumn, tok.line, tok.endColumn),
        options: {
          afterContentClassName: `nk-term-val-${tok.letter}`,
          stickiness: monaco.editor.TrackedRangeStickiness.NeverGrowsWhenTypingAtEdges,
        },
      });
    }
    el.textContent = css;
    liveDecoRef.current = ed.deltaDecorations(liveDecoRef.current, liveDecos);

    // Knob-Hover: alle Stellen des hovered Makros im Terminal hervorheben.
    const hoverDecos = hover
      ? knobTokens
          .filter((t) => t.letter === hover)
          .map((t) => ({
            range: new monaco.Range(t.line, t.startColumn, t.line, t.endColumn),
            options: {
              inlineClassName: "nk-term-knob-hover",
              stickiness: monaco.editor.TrackedRangeStickiness.NeverGrowsWhenTypingAtEdges,
            },
          }))
      : [];
    hoverDecoRef.current = ed.deltaDecorations(hoverDecoRef.current, hoverDecos);

    return () => {
      el.remove();
    };
  }, [knobs, hover, knobTokens, shown, editorReady]);

  const save = async () => {
    await applyPatchDraft();
  };

  const hostIr = useHostStore((s) => s.irSlots);
  const cabRows = useMemo(
    () => mergeIrSlots(irSlotsFromScript(baseScript), hostIr),
    [baseScript, hostIr],
  );

  return (
    <section
      className="flex h-full min-h-0 flex-1 flex-col bg-black"
      data-patch-model={editing ? PATCH_MODEL_URI : ""}
    >
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
      <div className="nk-term-workspace min-h-0 flex-1">
        {structure.length > 0 ? (
          <nav className="nk-term-structure" data-patch-structure="" aria-label="Patch structure">
            {structure.map((node) => (
              <button
                key={node.id}
                type="button"
                className="nk-term-structure-item"
                data-group={node.group}
                data-chip-id={node.id}
                aria-current={focusNodeId === node.id ? "true" : undefined}
                onClick={() => publishSelection({
                  nodeIds: [node.id],
                  primaryId: node.id,
                  origin: "nav",
                  intent: "mark",
                })}
              >
                {node.id}
              </button>
            ))}
          </nav>
        ) : null}
        <div
          className={`nk-term relative min-h-0 flex-1 overflow-hidden ${
            frame.frame === "accent" ? "nk-term--edit" : "nk-term--view"
          }`}
          data-mode={frame.mode}
        >
          <div className="nk-term-glow pointer-events-none" aria-hidden />
          <div className="nk-term-scan pointer-events-none" aria-hidden />
          <PatchEditor
            value={shown}
            shared={editing}
            readOnly={frame.readOnly}
            lineNumbers={frame.lineNumbers}
            caret={frame.caret}
            onChange={(v) => {
              if (editing) {
                setDraftScript(v);
              }
            }}
            onReady={onMount}
          />
        </div>
      </div>
    </section>
  );
}

export default HackView;
