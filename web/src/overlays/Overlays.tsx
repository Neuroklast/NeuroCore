import { useEffect, useRef, useState } from "react";
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
import { inspectArgFields, inspectBlurb, inspectControlGroups, inspectRows, inspectNode } from "./inspectModel";
import { InspectKnob, isLiteralNumber, isMacroOrExpression } from "./InspectKnob";
import { OptimizePanel } from "./OptimizePanel";
import { overlayBodyOverflow, overlayIsWide, overlayShowsHostClose, overlaySizesToContent } from "./overlayChrome";
import { useOverlayShell } from "./overlayMotion";
import { persistUi } from "../chrome/persistUi";
import { footerLicenseLabel } from "../chrome/footerLicense";
import { PresetExplorer } from "./PresetExplorer";
import { cancelDiscard, confirmDiscard } from "../presets/presetActions";
import { StagesPanel } from "./StagesPanel";
import { MsegEditor } from "./MsegEditor";
import { StepSeqEditor } from "./StepSeqEditor";
import { ValidatePanel } from "./ValidatePanel";

function Seg({
  value,
  options,
  onPick,
  label,
}: {
  value: string;
  options: Array<{ id: string; label: string }>;
  onPick: (id: string) => void;
  label: string;
}) {
  return (
    <div className="nk-settings-segment" role="group" aria-label={label}>
      {options.map((o) => (
        <button
          key={o.id}
          type="button"
          className={`nk-clip ${value === o.id ? "on" : ""}`}
          aria-pressed={value === o.id}
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
    const { knobs, enums } = inspectControlGroups(argFields);
    const bound = argFields.filter((f) => isMacroOrExpression(f.value));
    const formula = argFields.filter((f) => {
      if (node.type === "mseg" && f.key === "points") return false;
      if (node.type === "stepseq" && (f.key === "values" || f.key === "mutes")) {
        return false;
      }
      const v = f.value;
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
        {knobs.length > 0 ? (
          <div className="nk-inspect-controls">
            {knobs.map((f) => (
              <InspectKnob
                key={f.key}
                nodeId={node.id}
                nodeType={node.type}
                field={f}
                current={f.value}
                onCommit={onCommit}
                onReveal={() => onReveal(f.key)}
              />
            ))}
          </div>
        ) : null}
        {enums.length > 0 ? (
          <div className="nk-inspect-enums">
            {enums.map((f) => (
              <InspectKnob
                key={f.key}
                nodeId={node.id}
                nodeType={node.type}
                field={f}
                current={f.value}
                onCommit={onCommit}
                onReveal={() => onReveal(f.key)}
              />
            ))}
          </div>
        ) : null}

        {node.type === "mseg" ? <MsegEditor key={node.id} nodeId={node.id} args={node.args} /> : null}
        {node.type === "stepseq" ? <StepSeqEditor nodeId={node.id} args={node.args} /> : null}

        {(node.type === "env" || node.type === "adsr") && (
          <div className="border border-[var(--nk-line)] bg-black/60" data-envelope-editor={node.type}>
            <div className="text-[10px] tracking-widest text-muted uppercase px-2 py-1">
              {node.type === "adsr" ? "ADSR Envelope" : "Envelope"}
            </div>
            <div className="w-full h-28 flex items-end px-2 pb-1">
              <svg viewBox="0 0 100 40" preserveAspectRatio="xMidYMid meet" width="100%" height="100%" className="nk-inspect-curve" aria-hidden="true">
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
          <div className="p-2 space-y-1 text-[11px] max-h-[280px] overflow-auto">
            {rows.map((r) => (
              <div key={`${r.group}-${r.key}`} className="flex justify-between">
                <span className="text-muted">{r.key}</span>
                <span className="font-mono text-ink truncate max-w-[220px]">{r.value}</span>
              </div>
            ))}
          </div>
        </details>
      </div>
    );
  }

export function SettingsBody() {
  const tempo = useHostStore((s) => s.tempoSource);
  const bpm = useHostStore((s) => s.bpm);
  const motion = useHostStore((s) => s.motion);
  const theme = useHostStore((s) => s.theme) ?? "signal";
  const live = useHostStore((s) => s.live);
  const licensed = useHostStore((s) => s.licensed);
  const licenseEmail = useHostStore((s) => s.licenseEmail);
  const demoRemainSec = useHostStore((s) => s.demoRemainSec);
  const standalone = useHostStore((s) => s.standalone);
  const frameRate = useHostStore((s) => s.frameRate);
  const discardPrompt = useHostStore((s) => s.discardPrompt);
  const setOverlay = useHostStore((s) => s.setOverlay);
  const [bpmDraft, setBpmDraft] = useState(String(Math.round(bpm)));
  const editingBpm = useRef(false);
  const [tempoError, setTempoError] = useState("");
  const cancelBpmBlur = useRef(false);

  useEffect(() => {
    if (! editingBpm.current) setBpmDraft(String(Math.round(bpm)));
  }, [bpm]);

  const setTempo = (followHost: boolean) => {
    setTempoError("");
    void getNativeFunction("setUi")({ bpmFollow: followHost })
      .catch(() => setTempoError("Could not change tempo source."));
  };
  const commitBpm = () => {
    editingBpm.current = false;
    if (cancelBpmBlur.current) {
      cancelBpmBlur.current = false;
      return;
    }
    if (tempo !== "USER") return;
    const parsed = Number(bpmDraft);
    if (! bpmDraft.trim() || ! Number.isFinite(parsed)) {
      setBpmDraft(String(Math.round(bpm)));
      setTempoError("Enter a valid BPM value.");
      return;
    }
    const next = Math.max(20, Math.min(400, Math.round(parsed)));
    setBpmDraft(String(next));
    setTempoError("");
    void getNativeFunction("setUi")({ bpmUser: next })
      .catch(() => setTempoError("Could not update BPM."));
  };

  return (
    <div className="nk-settings" data-settings="">
      <p className="nk-settings-intro">These settings are shared by every NEUROKORE instance on this computer.</p>
      <section className="nk-settings-license" aria-label="License">
        <div className="nk-settings-license-status">
          <h3>LICENSE</h3>
          <p>{licensed ? (licenseEmail ? "Licensed to " + licenseEmail : "Licensed") : footerLicenseLabel(licensed, demoRemainSec)}</p>
        </div>
        <button type="button" className="nk-clip" onClick={() => setOverlay("license")}>Manage license</button>
      </section>
      <div className="nk-settings-grid">
        <section className="nk-settings-panel">
          <h3>PROCESSING</h3>
          <Seg label="Processing mode" value={live ? "live" : "studio"}
            options={[{ id: "studio", label: "Studio" }, { id: "live", label: "Live" }]}
            onPick={(id) => persistUi({ live: id === "live" })} />
          <p>Live keeps latency low when playing. Studio uses linear-phase oversampling and adds latency.</p>
        </section>
        <section className="nk-settings-panel">
          <h3>ANIMATION</h3>
          <Seg label="Animation" value={motion}
            options={[{ id: "full", label: "Full" }, { id: "reduced", label: "Reduced" }, { id: "off", label: "Off" }]}
            onPick={(id) => persistUi({ motion: id as typeof motion })} />
          <p>Reduced and Off limit visual motion, not audio processing.</p>
        </section>
        <section className="nk-settings-panel">
          <h3>FRAME RATE</h3>
          <Seg label="Interface frame rate" value={String(frameRate)}
            options={[{ id: "30", label: "30 FPS" }, { id: "60", label: "60 FPS" }]}
            onPick={(id) => persistUi({ frameRate: id === "30" ? 30 : 60 })} />
          <p>Limits visual updates. Audio processing is unaffected.</p>
        </section>
        <section className="nk-settings-panel">
          <h3>THEME</h3>
          <Seg label="Color theme" value={theme}
            options={themeIds().map((id) => ({ id, label: themeOf(id).label }))}
            onPick={(id) => { if (isThemeId(id)) persistUi({ theme: id }); }} />
        </section>
        <section className="nk-settings-panel">
          <h3>UNSAVED CHANGES</h3>
          <Seg label="Warn before discarding edits" value={discardPrompt ? "on" : "off"}
            options={[{ id: "on", label: "Warn me" }, { id: "off", label: "No warning" }]}
            onPick={(id) => persistUi({ discardPrompt: id === "on" })} />
          <p>When off, preset edits can be discarded without confirmation.</p>
        </section>
        <section className="nk-settings-panel" data-settings-tempo="">
          <h3>TEMPO</h3>
          {!standalone && <Seg label="Tempo source" value={tempo === "USER" ? "USER" : "HOST"}
            options={[{ id: "HOST", label: "Host" }, { id: "USER", label: "Manual" }]}
            onPick={(id) => setTempo(id === "HOST")} />}
          {standalone && <p>The standalone transport uses the manual BPM. Play/Pause/Stop are in the transport bar.</p>}
          <label className="nk-settings-bpm">
            <span>BPM</span>
            <input type="number" min={20} max={400} step={1}
              aria-label={tempo === "USER" ? "Manual BPM" : "Host BPM"}
              disabled={tempo !== "USER"}
              value={tempo === "USER" ? bpmDraft : String(Math.round(bpm))}
              onFocus={() => { editingBpm.current = true; }}
              onChange={(e) => setBpmDraft(e.target.value)}
              onBlur={commitBpm}
              onKeyDown={(e) => {
                if (e.key === "Enter") e.currentTarget.blur();
                if (e.key === "Escape") {
                  e.stopPropagation();
                  cancelBpmBlur.current = true;
                  setBpmDraft(String(Math.round(bpm)));
                  editingBpm.current = false;
                  e.currentTarget.blur();
                }
              }}
            />
          </label>
          <p>{tempo === "USER" ? "Manual tempo for synced effects." : "Following your host tempo. Switch to Manual to edit."}</p>
          {tempoError ? <p role="alert" className="nk-settings-error">{tempoError}</p> : null}
        </section>
        {standalone ? (
          <section className="nk-settings-panel">
            <h3>AUDIO DEVICE</h3>
            <button type="button" className="nk-clip nk-settings-action"
              onClick={() => void getNativeFunction("overlay")({ name: "audio" })}>
              Audio device settings
            </button>
            <p>Choose your device and sample rate for the standalone app.</p>
          </section>
        ) : null}
      </div>
      <div className="nk-settings-footer" aria-label="Information">
        <button type="button" className="nk-clip" onClick={() => setOverlay(settingsAboutTarget())}>About NEUROKORE</button>
        <button type="button" className="nk-clip" onClick={() => setOverlay("help")}>Help / Manual</button>
      </div>
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
