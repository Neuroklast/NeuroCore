import { useEffect, useState } from "react";
import { getNativeFunction } from "../bridge/juce";
import { hasJuceBridge } from "../bridge/juce";
import { setCircuitArg } from "../assemble/addBlock";
import { FunctionsPanel } from "../functions/FunctionsPanel";
import { HelpPanel } from "./HelpPanel";
import { useAstStore } from "../store/astStore";
import { useHostStore } from "../store/hostStore";
import { isThemeId, themeIds, themeOf } from "../theme/theme";
import { AboutPanel } from "./AboutPanel";
import { LicensePanel } from "./LicensePanel";
import { settingsAboutTarget } from "./aboutModel";
import { ImpulsePanel, irSlotAction, loadIrFile } from "./ImpulsePanel";
import { isIrSlotId } from "../presets/irSlots";
import { peakToDb } from "../bridge/telemetry";
import {
  clampInspectArg,
  inspectArgFields,
  inspectBlurb,
  inspectRows,
  inspectNoteChoices,
  inspectSnapNote,
  inspectValueFromNote,
  type InspectArgField,
} from "./inspectModel";
import { OptimizePanel } from "./OptimizePanel";
import { overlayBodyOverflow, overlayIsWide, overlayShowsHostClose, overlaySizesToContent } from "./overlayChrome";
import { useOverlayShell } from "./overlayMotion";
import { persistUi } from "../chrome/persistUi";
import { PresetExplorer } from "./PresetExplorer";
import { cancelDiscard, confirmDiscard } from "../presets/presetActions";
import { StagesPanel } from "./StagesPanel";
import { ValidatePanel } from "./ValidatePanel";

function Seg({
  value,
  options,
  onPick,
}: {
  value: string;
  options: Array<{ id: string; label: string }>;
  onPick: (id: string) => void;
}) {
  return (
    <div className="flex gap-1">
      {options.map((o) => (
        <button
          key={o.id}
          type="button"
          className={`nk-clip min-h-[32px] flex-1 ${value === o.id ? "on" : ""}`}
          onClick={() => onPick(o.id)}
        >
          {o.label}
        </button>
      ))}
    </div>
  );
}

function setNodeArg(nodeId: string, key: string, value: string): void {
  if (!hasJuceBridge()) {
    setCircuitArg(nodeId, key, value);
    return;
  }
  void getNativeFunction("graphOp")({
    origin: "canvas",
    op: "setArg",
    node: nodeId,
    key,
    value,
  });
}

function InspectArgControl({
  nodeId,
  type,
  field,
}: {
  nodeId: string;
  type: string;
  field: InspectArgField;
}) {
  const bpm = useHostStore((s) => s.bpm);
  const notes = field.timing ? inspectNoteChoices(field.unit, field.min, field.max, bpm) : [];
  const [mode, setMode] = useState<"value" | "note">("value");
  const fieldClass = "h-7 w-full border border-[var(--nk-line)] bg-black px-1 text-ink";
  if (field.kind === "enum") {
    return (
      <select
        className={fieldClass}
        value={field.options.includes(field.value) ? field.value : field.options[0]}
        onChange={(e) => setNodeArg(nodeId, field.key, e.target.value)}
      >
        {field.options.map((opt) => (
          <option key={opt} value={opt}>
            {opt}
          </option>
        ))}
      </select>
    );
  }
  const commit = (raw: string) => {
    const next = clampInspectArg(type, field.key, raw);
    setNodeArg(nodeId, field.key, next);
    return next;
  };
  const pickNote = (label: string) => {
    const mapped = inspectValueFromNote(label, field.unit, bpm);
    if (mapped != null) {
      commit(mapped);
    }
  };
  return (
    <div className="flex flex-col gap-1">
      {notes.length > 0 ? (
        <div className="flex gap-1">
          <button
            type="button"
            className={`nk-clip h-7 min-w-[52px] px-2 ${mode === "value" ? "on" : ""}`}
            onClick={() => setMode("value")}
          >
            VALUE
          </button>
          <button
            type="button"
            className={`nk-clip h-7 min-w-[52px] px-2 ${mode === "note" ? "on" : ""}`}
            onClick={() => setMode("note")}
          >
            NOTE
          </button>
        </div>
      ) : null}
      {mode === "note" && notes.length > 0 ? (
        <select
          className={fieldClass}
          value={inspectSnapNote(field.value, field.unit, field.min, field.max, bpm) ?? notes[0]}
          onChange={(e) => pickNote(e.target.value)}
        >
          {notes.map((opt) => (
            <option key={opt} value={opt}>
              {opt}
            </option>
          ))}
        </select>
      ) : (
        <div className="flex items-center gap-2">
          <input
            className={fieldClass}
            defaultValue={field.value}
            key={`${field.key}:${field.value}`}
            aria-label={field.key}
            onKeyDown={(e) => {
              if (e.key === "Enter") e.currentTarget.blur();
              if (e.key === "Escape") { e.currentTarget.value = field.value; e.currentTarget.blur(); }
            }}
            onBlur={(e) => {
              if (e.target.value !== field.value) e.target.value = commit(e.target.value);
            }}
          />
          {field.unit ? <span className="shrink-0 text-[11px] text-muted">{field.unit}</span> : null}
        </div>
      )}
    </div>
  );
}

export function InspectBody({ nodeId }: { nodeId: string | null }) {
  const ast = useAstStore((s) => s.ast);
  const peak = useHostStore((s) => (nodeId ? s.clips[nodeId] : undefined));
  const node = ast?.nodes.find((n) => n.id === nodeId);
  const rows = inspectRows(node, ast);
  if (! node) {
    return <p>No node selected.</p>;
  }
  const argFields = inspectArgFields(node);
  const groups = ["meta", "arg", "jack", "param", "var"] as const;
  const label: Record<(typeof groups)[number], string> = {
    meta: "NODE",
    arg: "PARAMETERS",
    jack: "JACKS",
    param: "BOUND KNOBS",
    var: "VARIABLES",
  };
  const blurb = inspectBlurb(node.type);
  const peakDb = peak != null && Number.isFinite(peak) ? peakToDb(peak) : null;
  return (
    <div className="flex flex-col gap-3 text-[12px]">
      {groups.map((g) => {
        if (g === "arg") {
          if (argFields.length === 0) {
            return null;
          }
          return (
            <section key={g}>
              <div className="mb-1 text-[11px] tracking-widest text-muted">{label.arg}</div>
              <div className="nk-inspect-fields">
                {argFields.map((f) => (
                  <label key={`arg-${f.key}`} className="nk-inspect-field">
                    <span>{f.key.replace(/[_-]/g, " ")}</span>
                    <InspectArgControl nodeId={node.id} type={node.type} field={f} />
                  </label>
                ))}
              </div>
            </section>
          );
        }
        const slice = rows.filter((r) => r.group === g);
        if (slice.length === 0) {
          return null;
        }
        return (
          <section key={g}>
            <div className="mb-1 text-[11px] tracking-widest text-muted">{label[g]}</div>
            <table className="w-full border-collapse">
              <tbody>
                {slice.map((r) => (
                  <tr key={`${g}-${r.key}`} className="border-b border-accent/20">
                    <td className="w-28 py-1 pr-2 text-muted">{r.key}</td>
                    <td className="py-1 text-ink">{r.value}</td>
                  </tr>
                ))}
              </tbody>
            </table>
          </section>
        );
      })}
      {peakDb != null ? (
        <section>
          <div className="mb-1 text-[11px] tracking-widest text-muted">LEVEL</div>
          <p className="text-ink tabular-nums">{peakDb.toFixed(1)} dB</p>
        </section>
      ) : null}
      {blurb ? (
        <section>
          <div className="mb-1 text-[11px] tracking-widest text-muted">ABOUT</div>
          <p className="text-ink">{blurb}</p>
        </section>
      ) : null}
      {isIrSlotId(node.id) || isIrSlotId(node.type) ? (
        <section>
          <div className="mb-1 text-[11px] tracking-widest text-muted">CABINET</div>
          <div className="flex flex-wrap gap-2">
            <button type="button" className="nk-clip" onClick={() => loadIrFile(node.id)}>Load</button>
            <button type="button" className="nk-clip" onClick={() => irSlotAction("preview", node.id)}>Play</button>
            <button type="button" className="nk-clip" onClick={() => irSlotAction("clear", node.id)}>Clear</button>
          </div>
        </section>
      ) : null}
    </div>
  );
}

function SettingsBody() {
  const tempo = useHostStore((s) => s.tempoSource);
  const bpm = useHostStore((s) => s.bpm);
  const motion = useHostStore((s) => s.motion);
  const cables = useHostStore((s) => s.cables);
  const theme = useHostStore((s) => s.theme) ?? "signal";
  const mode = useHostStore((s) => s.mode);
  const live = useHostStore((s) => s.live);
  const licensed = useHostStore((s) => s.licensed);
  const frameRate = useHostStore((s) => s.frameRate);
  const discardPrompt = useHostStore((s) => s.discardPrompt);
  const setOverlay = useHostStore((s) => s.setOverlay);
  return (
    <div className="flex flex-col gap-4 text-[13px]">
      <section>
        <div className="mb-1 text-[11px] tracking-widest text-ink">LICENSE</div>
        <div className="mb-2 text-[12px] text-muted">
          {licensed ? "Licensed" : "Demo — Mix goes dry after 14 days"}
        </div>
        <div className="flex gap-2">
          <button type="button" className="nk-clip flex-1" onClick={() => setOverlay("license")}>License…</button>
          <button type="button" className="nk-clip flex-1" onClick={() => void getNativeFunction("pickFile")({ kind: "license" })}>
            Install .lic
          </button>
        </div>
      </section>
      <section>
        <div className="mb-1 text-[11px] tracking-widest text-ink">ANIMATION</div>
        <Seg
          value={motion}
          options={[{ id: "full", label: "Full" }, { id: "reduced", label: "Reduced" }, { id: "off", label: "Off" }]}
          onPick={(id) => persistUi({ motion: id as typeof motion })}
        />
      </section>
      <section>
        <div className="mb-1 text-[11px] tracking-widest text-ink">FRAME RATE</div>
        <Seg
          value={String(frameRate === 30 ? 30 : 60)}
          options={[
            { id: "30", label: "30" },
            { id: "60", label: "60" },
          ]}
          onPick={(id) => persistUi({ frameRate: Number(id) === 30 ? 30 : 60 })}
        />
      </section>
      <section>
        <div className="mb-1 text-[11px] tracking-widest text-ink">UNSAVED PROMPT</div>
        <Seg
          value={discardPrompt ? "on" : "off"}
          options={[{ id: "on", label: "On" }, { id: "off", label: "Off" }]}
          onPick={(id) => persistUi({ discardPrompt: id === "on" })}
        />
      </section>
      <section>
        <div className="mb-1 text-[11px] tracking-widest text-ink">PROCESSING</div>
        <Seg
          value={live || mode === "LIVE" ? "live" : "studio"}
          options={[{ id: "studio", label: "Studio" }, { id: "live", label: "Live" }]}
          onPick={(id) => persistUi({ live: id === "live" })}
        />
        <p className="mt-2 text-[11px] leading-snug text-muted">
          Live keeps delay low so playing feels immediate. Studio uses linear-phase oversampling for mix and master work — more delay, the host reports it as latency. CPU in the status bar is 0–100. SAFE means the plugin went dry to protect the session, not a percent over 100.
        </p>
      </section>
      <section>
        <div className="mb-1 text-[11px] tracking-widest text-ink">THEME</div>
        <Seg
          value={theme}
          options={themeIds().map((id) => ({ id, label: themeOf(id).label }))}
          onPick={(id) => {
            if (! isThemeId(id)) {
              return;
            }
            persistUi({ theme: id });
          }}
        />
      </section>
      <section>
        <div className="mb-1 text-[11px] tracking-widest text-ink">CIRCUIT CABLES</div>
        <Seg
          value={cables}
          options={[{ id: "dots", label: "Dots" }, { id: "wave", label: "Wave" }]}
          onPick={(id) => persistUi({ cables: id as typeof cables })}
        />
      </section>
      <section>
        <div className="mb-1 text-[11px] tracking-widest text-ink">TEMPO</div>
        <Seg
          value={tempo === "USER" ? "USER" : "HOST"}
          options={[{ id: "HOST", label: "Host" }, { id: "USER", label: "User" }]}
          onPick={(id) => void getNativeFunction("setUi")({ bpmFollow: id === "HOST" })}
        />
        <label className="mt-2 flex items-center gap-2">
          BPM
          <input
            className="h-8 w-20 border border-accent bg-black px-2 text-ink"
            type="number"
            value={bpm}
            onChange={(e) => void getNativeFunction("setUi")({ bpmUser: Number(e.target.value) })}
          />
        </label>
      </section>
      <section>
        <div className="mb-1 text-[11px] tracking-widest text-ink">AUDIO</div>
        <button type="button" className="nk-clip" onClick={() => void getNativeFunction("overlay")({ name: "audio" })}>
          Audio device ...
        </button>
      </section>
      <section>
        <div className="mb-1 text-[11px] tracking-widest text-ink">ABOUT</div>
        <div className="flex gap-2">
          <button type="button" className="nk-clip flex-1" onClick={() => setOverlay(settingsAboutTarget())}>About</button>
          <button type="button" className="nk-clip flex-1" onClick={() => setOverlay("license")}>License</button>
          <button type="button" className="nk-clip flex-1" onClick={() => setOverlay("help")}>Help / Manual</button>
        </div>
      </section>
    </div>
  );
}

export function Overlays() {
  const overlay = useHostStore((s) => s.overlay);
  const motion = useHostStore((s) => s.motion);
  const setOverlay = useHostStore((s) => s.setOverlay);
  const inspectId = useHostStore((s) => s.inspectId);
  const shell = useOverlayShell(overlay, motion);
  const name = shell.name;

  useEffect(() => {
    if (! name) {
      return;
    }
    const onKey = (e: KeyboardEvent) => {
      if (e.key === "Escape") {
        if (name === "discard") {
          cancelDiscard();
          return;
        }
        setOverlay(null);
      }
    };
    window.addEventListener("keydown", onKey);
    return () => window.removeEventListener("keydown", onKey);
  }, [name, setOverlay]);

  if (! name) {
    return null;
  }

  const title = name === "settings" ? "Settings"
    : name === "presets" ? "Preset Explorer"
    : name === "functions" ? "Functions"
    : name === "stages" ? "Stages"
    : name === "help" ? "Help"
    : name === "license" ? "License"
    : name === "about" ? "About"
    : name === "validate" ? "Validate"
    : name === "optimize" ? "Optimize"
    : name === "ir" ? "Impulse"
    : name === "discard" ? "Unsaved"
    : "Inspect";

  return (
    <div
      className="nk-overlay-host"
      data-phase={shell.phase}
      onClick={() => {
        // Only close via X button for inspect overlay
        if (name === "inspect") {
          return;
        }
        if (name === "discard") {
          cancelDiscard();
          return;
        }
        setOverlay(null);
      }}
    >
      <div
        className={`nk-overlay flex w-full flex-col ${
          overlayIsWide(name)
            ? "h-[720px] max-h-[calc(100%-40px)] max-w-[1240px]"
            : "max-h-[calc(100%-40px)] max-w-[760px]"
        }`}
        data-phase={shell.phase}
        onClick={(e) => e.stopPropagation()}
      >
        <div className="nk-overlay-hud" aria-hidden>
          <span className="nk-overlay-corner nk-overlay-corner--tl" />
          <span className="nk-overlay-corner nk-overlay-corner--tr" />
          <span className="nk-overlay-corner nk-overlay-corner--bl" />
          <span className="nk-overlay-corner nk-overlay-corner--br" />
        </div>
        <div className="flex h-10 shrink-0 items-center justify-between border-b border-accent px-3">
          <span className="nk-overlay-led inline-block h-2 w-2 rounded-full bg-accent" />
          <span className="text-[14px] text-ink">{title}</span>
          <button
            type="button"
            className="nk-overlay-x"
            aria-label="Close"
            onClick={() => (name === "discard" ? cancelDiscard() : setOverlay(null))}
          />
        </div>
        <div className={`min-h-0 p-5 text-ink ${
          overlaySizesToContent(name) ? "" : "flex-1"
        } ${
          overlayBodyOverflow(name) === "hidden" ? "flex overflow-hidden" : "overflow-auto"
        }`}>
          {name === "presets" && <PresetExplorer />}

          {name === "functions" && <FunctionsPanel />}

          {name === "stages" && <StagesPanel />}

          {name === "settings" && <SettingsBody />}

          {name === "about" && <AboutPanel />}

          {name === "validate" && <ValidatePanel />}

          {name === "optimize" && <OptimizePanel />}

          {name === "help" && <HelpPanel />}

          {name === "license" && (
            <LicensePanel />
          )}

          {name === "ir" && (
            <ImpulsePanel focusSlot={inspectId} />
          )}

          {name === "inspect" && (
            <InspectBody nodeId={inspectId} />
          )}

          {name === "discard" && (
            <div className="flex flex-col gap-4 text-[13px]">
              <p>Discard unsaved plugin?</p>
              <div className="flex gap-2">
                <button type="button" className="nk-clip flex-1" onClick={() => cancelDiscard()}>Keep editing</button>
                <button type="button" className="nk-clip flex-1" onClick={() => void confirmDiscard()}>Discard</button>
              </div>
            </div>
          )}
        </div>
        {overlayShowsHostClose(name) ? (
          <div className="flex shrink-0 justify-end p-3">
            <button type="button" className="nk-clip" onClick={() => setOverlay(null)}>Close</button>
          </div>
        ) : null}
      </div>
    </div>
  );
}
