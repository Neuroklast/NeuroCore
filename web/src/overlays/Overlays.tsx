import { useEffect, useState } from "react";
import { getNativeFunction } from "../bridge/juce";
import { setCircuitArg } from "../assemble/addBlock";
import { FunctionsPanel } from "../functions/FunctionsPanel";
import { HelpPanel } from "./HelpPanel";
import { useAstStore } from "../store/astStore";
import { publishSelection } from "../store/patchWorkspace";
import { useHostStore } from "../store/hostStore";
import { useTelemetryStore } from "../store/telemetryStore";
import { isThemeId, themeIds, themeOf } from "../theme/theme";
import { AboutPanel } from "./AboutPanel";
import { LicensePanel } from "./LicensePanel";
import { settingsAboutTarget } from "./aboutModel";
import { ImpulsePanel } from "./ImpulsePanel";
import { chipFootPeak } from "../assemble/chipFace";
import { chipSpec } from "../assemble/chipSpec";
import { nodeMeter } from "../assemble/nodeMeter";
import { adsrEnvelopePath } from "../assemble/nodeFace/adsrEnvelope";
import { envEnvelopePath } from "../assemble/nodeFace/envEnvelope";
import { inspectArgFields, inspectBlurb, inspectRows, inspectNode } from "./inspectModel";
import { InspectKnob, isLiteralNumber, isMacroOrExpression } from "./InspectKnob";
import { OptimizePanel } from "./OptimizePanel";
import { overlayBodyOverflow, overlayIsWide, overlayShowsHostClose, overlaySizesToContent } from "./overlayChrome";
import { useOverlayShell } from "./overlayMotion";
import { persistUi } from "../chrome/persistUi";
import { PresetExplorer } from "./PresetExplorer";
import { cancelDiscard, confirmDiscard } from "../presets/presetActions";
import { StagesPanel } from "./StagesPanel";
import { StepSeqEditor } from "./StepSeqEditor";
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

export function InspectBody({ nodeId }: { nodeId: string | null }) {
    const ast = useAstStore((s) => s.ast);
    const clips = useHostStore((s) => s.clips);
    const clipsRms = useHostStore((s) => s.clipsRms);
    const inPeak = useTelemetryStore((s) => s.inPeak);
    const outPeak = useTelemetryStore((s) => s.outPeak);
    const inRms = useTelemetryStore((s) => s.inRms);
    const outRms = useTelemetryStore((s) => s.outRms);
    const [technicalOpen, setTechnicalOpen] = useState(false);
    const node = inspectNode(ast, nodeId);
    if (! node) {
      return <div className="nk-inspect-empty"><strong>Select a chip to inspect</strong><p>Double-click a component on the board to open its controls and signal readings.</p></div>;
    }
    const spec = chipSpec(node.type, node.args);
    const argFields = inspectArgFields(node);
    const rows = inspectRows(node, ast);
    const meter = nodeMeter({
      id: node.id,
      type: node.type,
      clips,
      clipsRms,
      pluginInPeak: inPeak,
      pluginOutPeak: outPeak,
      pluginInRms: inRms,
      pluginOutRms: outRms,
    });
    const terminal = node.type === "in" || node.type === "out";
    const blurb = terminal ? (node.type === "out" ? "Final plugin output sent to the host." : "Audio entering the plugin from the host.") : inspectBlurb(node.type);
    const editable = argFields.filter((f) => isLiteralNumber(node.args[f.key] ?? "") || f.options.length > 0);
    const bound = argFields.filter((f) => isMacroOrExpression(node.args[f.key] ?? ""));
    const formula = argFields.filter((f) => {
      if (node.type === "stepseq" && (f.key === "values" || f.key === "mutes")) {
        return false;
      }
      const v = node.args[f.key] ?? "";
      return ! isLiteralNumber(v) && ! isMacroOrExpression(v) && f.options.length === 0;
    });
    const detector = argFields.find((f) => f.key === "type" || f.key === "detector");
    const detectorLabel = detector ? ` · ${(node.args[detector.key] ?? "").toUpperCase()}` : "";

    const onCommit = (id: string, key: string, value: string) => {
      setCircuitArg(id, key, value);
    };
    const onReveal = (key: string) => {
      publishSelection({ nodeIds: [node.id], primaryId: node.id, param: key, origin: "inspect", intent: "reveal" });
    };

    return (
      <div className="nk-inspect" data-inspect-node={node.id}>
        {/* Header: wie ein Hardware-Modul bedient */}
        <div className="nk-inspect-heading">
          <div className="nk-inspect-name">{terminal ? (node.type === "out" ? "OUTPUT" : "INPUT") : spec.label.toUpperCase()}</div>
          <div className="nk-inspect-identity">{node.id} · {node.busName || "main"}{detectorLabel}</div>
          {blurb ? <p className="nk-inspect-description">{blurb}</p> : null}
        </div>

        {/* Primäre Bedienelemente: kompakte Regler + Werte */}
        {editable.length > 0 ? (
          <div className="nk-inspect-controls">
            {editable.map((f) => (
              <div key={f.key} className="flex flex-col gap-1">
                <InspectKnob
                  nodeId={node.id}
                  nodeType={node.type}
                  field={f}
                  current={node.args[f.key]}
                  onCommit={onCommit}
                  onReveal={() => onReveal(f.key)}
                />
              </div>
            ))}
          </div>
        ) : null}

        {node.type === "stepseq" ? <StepSeqEditor nodeId={node.id} args={node.args} /> : null}

        {(node.type === "env" || node.type === "adsr") && (
          <div className="border border-[var(--nk-line)] bg-black/60" data-envelope-editor={node.type}>
            <div className="text-[10px] tracking-widest text-muted uppercase px-2 py-1">
              {node.type === "adsr" ? "ADSR Envelope" : "Envelope"}
            </div>
            <div className="w-full h-28 flex items-end px-2 pb-1">
              <svg viewBox="0 0 100 40" preserveAspectRatio="none" width="100%" height="40" className="nk-inspect-curve">
                <path
                  d={node.type === "adsr" ? adsrEnvelopePath(node.args) : envEnvelopePath(node.args)}
                  stroke="var(--nk-cyan)"
                  fill="none"
                  strokeWidth="1.5"
                />
              </svg>
            </div>
          </div>
        )}

        <section className="nk-inspect-signal" aria-label="Signal readings">
          <div className="nk-inspect-signal-title">{meter.sourceLabel}</div>
          <div className="nk-inspect-readings">
            <div><span>PEAK</span><strong>{meter.peak == null ? "—" : chipFootPeak(meter.peak)}</strong></div>
            <div><span>RMS</span><strong>{meter.rms == null ? "—" : chipFootPeak(meter.rms)}</strong></div>
          </div>
          <div className="nk-inspect-tap">{meter.waveLabel}</div>
        </section>

        {/* Makro-/Ausdruck-Bindungen (schreibgeschützt, mit Navigationshinweis) */}
        {bound.length > 0 ? (
          <section className="border border-[var(--nk-line)] bg-black/40">
            <div className="text-[10px] tracking-widest text-muted uppercase px-2 py-1">Macro / Expression</div>
            <div className="p-1 space-y-1">
              {bound.map((f) => (
                <div key={f.key} className="flex items-center gap-2 px-2 py-1 text-[10px]">
                  <span className="w-16 font-mono text-cyan">{f.key}</span>
                  <span className="flex-1 font-mono text-ink truncate">{node.args[f.key]}</span>
                  <button type="button" className="nk-inspect-bind text-[9px] text-muted" onClick={() => onReveal(f.key)}>
                    Reveal
                  </button>
                </div>
              ))}
            </div>
          </section>
        ) : null}

        {/* Formel-Parameter (schreibgeschützt, kein Makro, kein Literal) */}
        {formula.length > 0 ? (
          <section className="border border-[var(--nk-line)] bg-black/40">
            <div className="text-[10px] tracking-widest text-muted uppercase px-2 py-1">Formula</div>
            <div className="p-1 space-y-1">
              {formula.map((f) => (
                <div key={f.key} className="flex items-center gap-2 px-2 py-1 text-[10px]">
                  <span className="w-16 font-mono text-cyan">{f.key}</span>
                  <span className="flex-1 font-mono text-ink truncate">{node.args[f.key]}</span>
                </div>
              ))}
            </div>
          </section>
        ) : null}

        {/* Technische Details: eingeklappt, kein dominierender Tisch */}
        <details
          className="border border-[var(--nk-line)] bg-black/40"
          open={technicalOpen}
          onToggle={(event) => setTechnicalOpen(event.currentTarget.open)}
        >
          <summary className="cursor-pointer select-none text-[10px] tracking-widest text-muted uppercase px-2 py-1">
            Technical Details
          </summary>
          <div className="p-2 space-y-1 text-[9px]">
            {rows.slice(0, 14).map((r) => (
              <div key={`${r.group}-${r.key}`} className="flex justify-between">
                <span className="text-muted">{r.key}</span>
                <span className="font-mono text-ink truncate max-w-[220px]">{r.value}</span>
              </div>
            ))}
            {rows.length > 14 && <div className="text-muted">… more entries</div>}
          </div>
        </details>
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
        role="dialog" aria-modal="true" aria-label={title}
        className={`nk-overlay ${name === "inspect" ? "nk-overlay--inspect" : ""} flex w-full flex-col ${
          overlayIsWide(name)
            ? "nk-overlay--stage max-w-[1240px]"
            : "max-w-[760px]"
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
        <div className="nk-overlay-titlebar flex h-10 shrink-0 items-center justify-between border-b border-accent px-3">
          <span className="nk-overlay-led inline-block h-2 w-2 rounded-full bg-accent" />
          <span className="nk-overlay-caption text-[14px] text-ink">{title}</span>
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
