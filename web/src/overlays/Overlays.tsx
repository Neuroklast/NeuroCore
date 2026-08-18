import { useEffect } from "react";
import { getNativeFunction } from "../bridge/juce";
import { FunctionsPanel } from "../functions/FunctionsPanel";
import { HelpPanel } from "./HelpPanel";
import { useAstStore } from "../store/astStore";
import { useHostStore } from "../store/hostStore";
import { AboutPanel } from "./AboutPanel";
import { settingsAboutTarget } from "./aboutModel";
import { inspectRows } from "./inspectModel";
import { OptimizePanel } from "./OptimizePanel";
import { overlayBodyOverflow, overlayIsWide, overlayShowsHostClose } from "./overlayChrome";
import { useOverlayShell } from "./overlayMotion";
import { PresetExplorer } from "./PresetExplorer";
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

function InspectBody({ nodeId }: { nodeId: string | null }) {
  const ast = useAstStore((s) => s.ast);
  const node = ast?.nodes.find((n) => n.id === nodeId);
  const rows = inspectRows(node, ast);
  if (! node) {
    return <p>No node selected.</p>;
  }
  const groups = ["meta", "arg", "jack", "param", "var"] as const;
  const label: Record<(typeof groups)[number], string> = {
    meta: "NODE",
    arg: "PARAMETERS",
    jack: "JACKS",
    param: "BOUND KNOBS",
    var: "VARIABLES",
  };
  return (
    <div className="flex flex-col gap-3 text-[12px]">
      {groups.map((g) => {
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
                    <td className="py-1 text-ink">
                      {g === "arg" ? (
                        <input
                          className="h-7 w-full border border-accent/40 bg-black px-1 text-accent"
                          defaultValue={r.value}
                          onBlur={(e) => {
                            void getNativeFunction("graphOp")({
                              origin: "canvas",
                              op: "setArg",
                              node: node.id,
                              key: r.key,
                              value: e.target.value,
                            });
                          }}
                        />
                      ) : r.value}
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          </section>
        );
      })}
    </div>
  );
}

function SettingsBody() {
  const scale = useHostStore((s) => s.scale);
  const tempo = useHostStore((s) => s.tempoSource);
  const bpm = useHostStore((s) => s.bpm);
  const motion = useHostStore((s) => s.motion);
  const cables = useHostStore((s) => s.cables);
  const formulaPt = useHostStore((s) => s.formulaPt);
  const mode = useHostStore((s) => s.mode);
  const setOverlay = useHostStore((s) => s.setOverlay);
  return (
    <div className="flex flex-col gap-4 text-[13px]">
      <section>
        <div className="mb-1 text-[11px] tracking-widest text-ink">ANIMATION</div>
        <Seg
          value={motion}
          options={[{ id: "full", label: "Full" }, { id: "reduced", label: "Reduced" }, { id: "off", label: "Off" }]}
          onPick={(id) => useHostStore.setState({ motion: id as typeof motion })}
        />
      </section>
      <section>
        <div className="mb-1 text-[11px] tracking-widest text-ink">PROCESSING</div>
        <Seg
          value={mode === "LIVE" ? "live" : "studio"}
          options={[{ id: "studio", label: "Studio" }, { id: "live", label: "Live" }]}
          onPick={(id) => {
            const next = id === "live" ? "LIVE" : "STUDIO";
            useHostStore.setState({ mode: next });
            void getNativeFunction("setChoice")({ id: "mode", index: id === "live" ? 1 : 0 });
          }}
        />
      </section>
      <section>
        <div className="mb-1 text-[11px] tracking-widest text-ink">DISPLAY</div>
        <Seg
          value={String(scale)}
          options={[{ id: "100", label: "100%" }, { id: "125", label: "125%" }, { id: "150", label: "150%" }]}
          onPick={(id) => void getNativeFunction("setUi")({ scale: Number(id) })}
        />
        <div className="mt-2 flex items-center gap-2">
          <span>Formula text size</span>
          <button type="button" className="nk-clip" onClick={() => useHostStore.setState({ formulaPt: Math.max(12, formulaPt - 1) })}>Aa-</button>
          <span>{formulaPt} pt</span>
          <button type="button" className="nk-clip" onClick={() => useHostStore.setState({ formulaPt: Math.min(24, formulaPt + 1) })}>Aa+</button>
        </div>
      </section>
      <section>
        <div className="mb-1 text-[11px] tracking-widest text-ink">CIRCUIT CABLES</div>
        <Seg
          value={cables}
          options={[{ id: "dots", label: "Dots" }, { id: "wave", label: "Wave" }]}
          onPick={(id) => useHostStore.setState({ cables: id as typeof cables })}
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
  const irSlots = useHostStore((s) => s.irSlots);
  const inspectId = useHostStore((s) => s.inspectId);
  const licensed = useHostStore((s) => s.licensed);
  const shell = useOverlayShell(overlay, motion);
  const name = shell.name;

  useEffect(() => {
    if (! name) {
      return;
    }
    const onKey = (e: KeyboardEvent) => {
      if (e.key === "Escape") {
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
    : "Inspect";

  return (
    <div
      className="nk-overlay-host"
      data-phase={shell.phase}
      onClick={() => setOverlay(null)}
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
        <div className="flex h-10 shrink-0 items-center justify-between border-b border-accent px-3">
          <span className="nk-overlay-led inline-block h-2 w-2 rounded-full bg-accent" />
          <span className="text-[14px] text-ink">{title}</span>
          <button type="button" className="nk-clip px-3" onClick={() => setOverlay(null)}>X</button>
        </div>
        <div className={`min-h-0 flex-1 p-5 text-ink ${
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
            <div>
              <p>{licensed ? "Licensed" : "Demo / unlicensed"}</p>
              <button type="button" className="nk-clip mt-3" onClick={() => void getNativeFunction("pickFile")({ kind: "license" })}>
                Install .lic
              </button>
            </div>
          )}

          {name === "ir" && (
            <ul>
              {irSlots.map((s) => (
                <li key={s.slot} className="mb-2 flex items-center gap-2">
                  <span>{s.slot}: {s.loaded ? s.name : "empty"}</span>
                  <button type="button" className="nk-clip" onClick={() => void getNativeFunction("pickFile")({ kind: "ir", slot: s.slot })}>
                    Load
                  </button>
                </li>
              ))}
            </ul>
          )}

          {name === "inspect" && (
            <InspectBody nodeId={inspectId} />
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
