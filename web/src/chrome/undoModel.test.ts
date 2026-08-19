import { describe, expect, it } from "vitest";
import { isRedoKey, isUndoKey, undoTargetIsText } from "./undoModel";

describe("formula undo chords", () => {
  it("maps Ctrl+Z / Ctrl+Y / Shift+Ctrl+Z and leaves typing alone", () => {
    expect(isUndoKey({ key: "z", ctrlKey: true, metaKey: false, altKey: false, shiftKey: false })).toBe(true);
    expect(isRedoKey({ key: "y", ctrlKey: true, metaKey: false, altKey: false, shiftKey: false })).toBe(true);
    expect(isRedoKey({ key: "z", ctrlKey: true, metaKey: false, altKey: false, shiftKey: true })).toBe(true);
    expect(isUndoKey({ key: "z", ctrlKey: false, metaKey: false, altKey: false, shiftKey: false })).toBe(false);
  });

  it("does not steal undo from a text field", () => {
    expect(undoTargetIsText({ tagName: "INPUT" })).toBe(true);
    expect(undoTargetIsText({ tagName: "TEXTAREA" })).toBe(true);
    expect(undoTargetIsText({ tagName: "DIV" })).toBe(false);
    expect(undoTargetIsText(null)).toBe(false);
  });

  it("recognises a Monaco editor container as a text target (Issue 2: Ctrl+A)", () => {
    // Monaco renders inside a div that has class 'monaco-editor'.
    // undoTargetIsText must return true so that the global keydown handler does
    // NOT block Ctrl+A for that element.
    const monacoEl = {
      tagName: "DIV",
      isContentEditable: false,
      closest: (sel: string) => sel.includes("monaco-editor") ? {} : null,
    };
    expect(undoTargetIsText(monacoEl)).toBe(true);
  });
});
