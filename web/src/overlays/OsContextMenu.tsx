import { useEffect, useRef, useState, type ReactNode } from "react";
import { ADD_CATEGORIES } from "../assemble/addBlock";
import { addPickerBlocks } from "./addPicker";

export function OsContextMenu({
  left,
  top,
  title,
  children,
  onDismiss,
}: {
  left: number;
  top: number;
  title?: string;
  children: ReactNode;
  onDismiss: () => void;
}) {
  const root = useRef<HTMLDivElement>(null);
  useEffect(() => {
    const close = (e: PointerEvent) => {
      if (root.current && ! root.current.contains(e.target as Node)) {
        onDismiss();
      }
    };
    const onKey = (e: KeyboardEvent) => {
      if (e.key === "Escape") {
        onDismiss();
      }
    };
    const id = window.requestAnimationFrame(() => {
      window.addEventListener("pointerdown", close, true);
    });
    window.addEventListener("keydown", onKey);
    return () => {
      window.cancelAnimationFrame(id);
      window.removeEventListener("pointerdown", close, true);
      window.removeEventListener("keydown", onKey);
    };
  }, [onDismiss]);

  return (
    <div ref={root} className="nk-ctx" style={{ left, top }} onPointerDown={(e) => e.stopPropagation()}>
      {title ? <div className="nk-ctx-id">{title}</div> : null}
      {children}
    </div>
  );
}

export function OsMenuItem({
  children,
  onClick,
}: {
  children: ReactNode;
  onClick: () => void;
}) {
  return (
    <button type="button" className="nk-ctx-item" onClick={onClick}>
      {children}
    </button>
  );
}

export function OsMenuSub({
  label,
  children,
}: {
  label: string;
  children: ReactNode;
}) {
  const [open, setOpen] = useState(false);
  return (
    <div
      className="nk-ctx-sub"
      onPointerEnter={() => setOpen(true)}
      onPointerLeave={() => setOpen(false)}
    >
      <button
        type="button"
        className="nk-ctx-item nk-ctx-parent"
        aria-expanded={open}
        onClick={() => setOpen((v) => ! v)}
      >
        {label}
        <span className="nk-ctx-caret">▸</span>
      </button>
      {open ? <div className="nk-ctx-flyout">{children}</div> : null}
    </div>
  );
}

export function OsAddPicker({
  onPick,
}: {
  onPick: (type: string, args: string) => void;
}) {
  const [cat, setCat] = useState<string>(ADD_CATEGORIES[0]);
  return (
    <div className="nk-add-pick" data-add-picker="1">
      <div className="nk-add-cats">
        {ADD_CATEGORIES.map((c) => (
          <button
            key={c}
            type="button"
            className={`nk-ctx-item ${c === cat ? "nk-add-on" : ""}`}
            onClick={() => setCat(c)}
          >
            {c}
          </button>
        ))}
      </div>
      <div className="nk-add-list">
        {addPickerBlocks(cat).map((b) => (
          <button
            key={`${b.label}:${b.args}`}
            type="button"
            className="nk-ctx-item"
            onClick={() => onPick(b.type, b.args)}
          >
            {b.label}
          </button>
        ))}
      </div>
    </div>
  );
}
