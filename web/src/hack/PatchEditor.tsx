import Editor, { type OnMount } from "@monaco-editor/react";
import { useEffect, useRef } from "react";
import "./monacoEnv";
import { useHostStore } from "../store/hostStore";
import { publishSelection } from "../store/patchWorkspace";
import { useTheme } from "../theme/themeBind";
import { nodeAtPosition, paramAtPosition, busFoldRanges } from "./dslSourceMap";
import {
  capturePatchEditorView,
  PATCH_MODEL_URI,
  readPatchEditorView,
} from "./patchEditorSession";
import {
  defineDslTheme,
  DSL_LANGUAGE_ID,
  DSL_THEME,
  registerNeurokoreDsl,
} from "./registerDsl";

export function PatchEditor({
  value,
  onChange,
  readOnly,
  shared,
  lineNumbers = "on",
  caret = true,
  onReady,
}: {
  value: string;
  onChange?: (value: string) => void;
  readOnly: boolean;
  shared: boolean;
  lineNumbers?: "on" | "off";
  caret?: boolean;
  onReady?: OnMount;
}) {
  const theme = useTheme();
  const fontSize = useHostStore((s) => s.formulaPt);
  const monacoRef = useRef<typeof import("monaco-editor") | null>(null);

  useEffect(() => {
    const monaco = monacoRef.current;
    if (! monaco) {
      return;
    }
    defineDslTheme(monaco, theme);
    monaco.editor.setTheme(DSL_THEME);
  }, [theme]);

  const onMount: OnMount = (ed, monaco) => {
    monacoRef.current = monaco;
    registerNeurokoreDsl(monaco);
    defineDslTheme(monaco, theme);
    monaco.editor.setTheme(DSL_THEME);
    monaco.languages.registerFoldingRangeProvider(DSL_LANGUAGE_ID, {
      provideFoldingRanges: (model) => {
        const ranges = busFoldRanges(model.getValue());
        return ranges.map((r) => ({
          start: r.startLine,
          end: r.endLine,
        }));
      },
    });
    if (shared) {
      const v = readPatchEditorView();
      ed.setPosition({ lineNumber: v.cursorLine, column: v.cursorColumn });
      ed.setScrollTop(v.scrollTop);
      ed.setScrollLeft(v.scrollLeft);
    }
    ed.onDidChangeCursorPosition((e) => {
      capturePatchEditorView({
        cursorLine: e.position.lineNumber,
        cursorColumn: e.position.column,
      });
      if (readOnly) {
        return;
      }
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
    ed.onDidScrollChange((e) => {
      capturePatchEditorView({ scrollTop: e.scrollTop, scrollLeft: e.scrollLeft });
    });
    onReady?.(ed, monaco);
  };

  return (
    <Editor
      height="100%"
      language={DSL_LANGUAGE_ID}
      theme={DSL_THEME}
      path={shared ? PATCH_MODEL_URI : undefined}
      loading={null}
      value={value}
      onChange={(v) => onChange?.(v ?? "")}
      onMount={onMount}
      options={{
        readOnly,
        minimap: { enabled: false },
        fontFamily: "JetBrains Mono, ui-monospace, Consolas, monospace",
        fontSize,
        lineNumbers,
        glyphMargin: true,
        folding: true,
        lineDecorationsWidth: readOnly ? 0 : 10,
        lineNumbersMinChars: readOnly ? 2 : 3,
        padding: { top: 10, bottom: 10 },
        scrollBeyondLastLine: false,
        wordWrap: "on",
        tabSize: 2,
        automaticLayout: true,
        snippetSuggestions: "top",
        tabCompletion: "on",
        suggest: { snippetsPreventQuickSuggestions: false, showSnippets: true },
        cursorStyle: "line",
        cursorWidth: caret ? 2 : 0,
        renderLineHighlight: caret ? "line" : "none",
        overviewRulerLanes: 0,
      }}
    />
  );
}

export default PatchEditor;
