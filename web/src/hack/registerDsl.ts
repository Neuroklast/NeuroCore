import type { Monaco } from "@monaco-editor/react";
import { complete } from "./dslComplete";
import { dslMonarch } from "./dslLanguage";

const LANG = "neurokore-dsl";
let registered = false;

export function registerNeurokoreDsl(monaco: Monaco): void {
  if (registered) {
    return;
  }
  registered = true;
  monaco.languages.register({ id: LANG });
  monaco.languages.setMonarchTokensProvider(LANG, dslMonarch as never);
  monaco.languages.setLanguageConfiguration(LANG, {
    comments: { lineComment: "#" },
    brackets: [["{", "}"], ["[", "]"], ["(", ")"]],
    autoClosingPairs: [
      { open: "{", close: "}" },
      { open: "[", close: "]" },
      { open: "(", close: ")" },
    ],
  });
  monaco.languages.registerCompletionItemProvider(LANG, {
    triggerCharacters: [" ", "=", ";"],
    provideCompletionItems(model, position) {
      const text = model.getValue();
      const offset = model.getOffsetAt(position);
      const items = complete(text, offset);
      const word = model.getWordUntilPosition(position);
      const range = {
        startLineNumber: position.lineNumber,
        endLineNumber: position.lineNumber,
        startColumn: word.startColumn,
        endColumn: word.endColumn,
      };
      return {
        suggestions: items.map((it) => ({
          label: it.label,
          insertText: it.insertText,
          detail: it.detail,
          kind: it.kind === "snippet"
            ? monaco.languages.CompletionItemKind.Snippet
            : it.kind === "property"
              ? monaco.languages.CompletionItemKind.Property
              : it.kind === "value"
                ? monaco.languages.CompletionItemKind.Value
                : monaco.languages.CompletionItemKind.Keyword,
          range,
        })),
      };
    },
  });
}

export const DSL_LANGUAGE_ID = LANG;

export const DSL_THEME = "neurokore-dark";

export function defineDslTheme(monaco: Monaco): void {
  monaco.editor.defineTheme(DSL_THEME, {
    base: "vs-dark",
    inherit: true,
    rules: [
      { token: "comment", foreground: "c8c8c8" },
      { token: "keyword", foreground: "ff1a1a" },
      { token: "knob", foreground: "ff1a1a" },
      { token: "number", foreground: "fff5f5" },
      { token: "identifier", foreground: "fff5f5" },
      { token: "operator", foreground: "c8c8c8" },
    ],
    colors: {
      "editor.background": "#000000",
      "editor.foreground": "#fff5f5",
      "editorLineNumber.foreground": "#c8c8c8",
      "editorCursor.foreground": "#ff003c",
      "editor.selectionBackground": "#ff003c33",
      "editorError.foreground": "#ff6b6b",
    },
  });
}
