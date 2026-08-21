import { getNativeFunction, hasJuceBridge } from "../bridge/juce";
import { presetAction } from "../presets/presetActions";
import { useHostStore } from "../store/hostStore";
import { nk } from "../theme/tokens";
import { Knob } from "./Knob";
import { footerLicenseLabel } from "./footerLicense";
import { toolbarSlots, workspaceTabClass } from "./toolbarChrome";

export function Hud() {
  const licensed = useHostStore((s) => s.licensed);
  return (
    <div className="flex h-5 shrink-0 items-center justify-between px-2 font-mono text-[11px] tracking-[0.16em] text-muted">
      <span>
        {nk.osBanner}
        {licensed ? " // LINK ACTIVE" : " // DEMO"}
      </span>
      <span className="text-muted">{nk.version}</span>
    </div>
  );
}

export function Toolbar() {
  const name = useHostStore((s) => s.presetName);
  const mode = useHostStore((s) => s.mode);
  const mix = useHostStore((s) => s.mix);
  const setOverlay = useHostStore((s) => s.setOverlay);
  const bypassed = mix <= 1e-5;
  return (
    <header className="flex h-[52px] w-full min-w-0 shrink-0 items-center gap-2 overflow-hidden px-3">
      <a
        href="https://neuroklast.net"
        target="_blank"
        rel="noreferrer"
        className="mr-2 flex shrink-0 items-center gap-3 no-underline"
      >
        <img src="./img/nk_logo.png" alt="" className="h-11 w-auto" />
        <span className="leading-tight">
          <span className="block font-brand text-[18px] tracking-wide text-ink">{nk.product}</span>
          <span className="block text-[12px] text-ink">
            {nk.byline} <span className="text-muted">{nk.version}</span>
          </span>
        </span>
      </a>
      {toolbarSlots().map((slot) => {
        if (slot.id === "presetPrev") {
          return (
            <button key={slot.id} type="button" className="nk-clip shrink-0 px-2" onClick={() => void presetAction({ action: "prev" })}>
              &lt;
            </button>
          );
        }
        if (slot.id === "presetNow") {
          return (
            <button
              key={slot.id}
              type="button"
              className={slot.className}
              title="Open preset explorer"
              onClick={() => setOverlay(slot.opens)}
            >
              {name || "untitled"}
            </button>
          );
        }
        if (slot.id === "presetNext") {
          return (
            <button key={slot.id} type="button" className="nk-clip shrink-0 px-2" onClick={() => void presetAction({ action: "next" })}>
              &gt;
            </button>
          );
        }
        if (slot.id === "mode") {
          const pick = (next: "LIVE" | "STUDIO") => {
            useHostStore.setState({ mode: next });
            void getNativeFunction("setChoice")({ id: "mode", index: next === "LIVE" ? 1 : 0 });
          };
          return (
            <div key={slot.id} className="nk-mode-pair" role="group" aria-label="Mode">
              <button
                type="button"
                className={mode === "LIVE" ? "on" : ""}
                aria-pressed={mode === "LIVE"}
                onClick={() => pick("LIVE")}
              >
                LIVE
              </button>
              <button
                type="button"
                className={mode === "STUDIO" || mode === "SAFE" ? "on" : ""}
                aria-pressed={mode !== "LIVE"}
                onClick={() => pick("STUDIO")}
              >
                STUDIO
              </button>
            </div>
          );
        }
        if (slot.id === "bypass") {
          return (
            <button
              key={slot.id}
              type="button"
              className={`nk-clip nk-alert shrink-0 text-[12px] ${bypassed ? "on" : ""}`}
              onClick={() => {
                const value = useHostStore.getState().toggleBypass();
                if (hasJuceBridge()) {
                  void getNativeFunction("setParam")({ id: "dryWet", value, gesture: "change" });
                }
              }}
            >
              BYPASS
            </button>
          );
        }
        const label = slot.id === "functions" ? "Functions"
          : slot.id === "stages" ? "Stages"
            : slot.id === "settings" ? "Settings" : "Help";
        return (
          <button key={slot.id} type="button" className="nk-clip shrink-0 text-[13px]" onClick={() => setOverlay(slot.id)}>
            {label}
          </button>
        );
      })}
    </header>
  );
}

export function InputSwitch() {
  const input = useHostStore((s) => s.input);
  return (
    <div className="grid grid-cols-3 gap-px">
      {(["L", "BOTH", "R"] as const).map((lab, i) => (
        <button
          key={lab}
          type="button"
          className={`min-h-[26px] border bg-surface-high text-[11px] font-brand ${
            input === i ? "border-[var(--nk-line)] bg-surface text-ink" : "border-[var(--nk-line)] text-muted"
          }`}
          onClick={() => {
            useHostStore.getState().setInput(i);
            if (hasJuceBridge()) {
              void getNativeFunction("setChoice")({ id: "input", index: i });
            }
          }}
        >
          {lab}
        </button>
      ))}
    </div>
  );
}

export function Knobs({ bind = true, rail = "left" }: { bind?: boolean; rail?: "left" | "bottom" }) {
  const knobs = useHostStore((s) => s.knobs);
  if (rail === "bottom") {
    return (
      <div className="flex h-[148px] shrink-0 items-stretch gap-px border-t border-[var(--nk-line)]">
        {knobs.map((k) => (
          <div key={k.id} className="min-w-0 flex-1">
            <Knob knob={k} bind={bind} compact />
          </div>
        ))}
      </div>
    );
  }
  return (
    <div className="flex min-h-0 flex-1 flex-col">
      {knobs.map((k) => (
        <div key={k.id} className="min-h-0 flex-1">
          <Knob knob={k} bind={bind} />
        </div>
      ))}
    </div>
  );
}

export function WorkspaceTabs({
  workspace,
  setWorkspace,
}: {
  workspace: "face" | "assemble" | "hack";
  setWorkspace: (w: "face" | "assemble" | "hack") => void;
}) {
  return (
    <div className="flex h-[26px] shrink-0">
      {([
        { id: "face" as const, label: "Unit" },
        { id: "assemble" as const, label: "Circuit" },
        { id: "hack" as const, label: "Terminal" },
      ]).map((t) => (
        <button
          key={t.id}
          type="button"
          className={workspaceTabClass(workspace === t.id)}
          onClick={() => setWorkspace(t.id)}
        >
          {t.label}
        </button>
      ))}
    </div>
  );
}

export function MixOs() {
  const mix = useHostStore((s) => s.mix);
  const os = useHostStore((s) => s.os);
  const polisher = useHostStore((s) => s.polisher);
  return (
    <div className="nk-macro-rule flex h-[30px] shrink-0 items-center gap-3 border-t border-b border-[var(--nk-line)] bg-surface px-1 text-[11px]">
      <span className="font-brand text-muted">Input channel</span>
      <InputSwitch />
      <span className="font-brand text-muted">Oversampling</span>
      <select
        className="h-[26px] border border-[var(--nk-line)] bg-surface-high px-1 text-ink"
        value={os}
        onChange={(e) => {
          const index = Number(e.target.value);
          useHostStore.getState().setOs(index);
          if (hasJuceBridge()) {
            void getNativeFunction("setChoice")({ id: "os", index });
          }
        }}
      >
        <option value={0}>1x</option>
        <option value={1}>2x</option>
        <option value={2}>4x</option>
        <option value={3}>8x</option>
      </select>
      <span className="font-brand text-muted">Soft Clip</span>
      <select
        className="h-[26px] border border-[var(--nk-line)] bg-surface-high px-1 text-ink"
        value={polisher}
        onChange={(e) => {
          const index = Number(e.target.value);
          useHostStore.getState().setPolisher(index);
          if (hasJuceBridge()) {
            void getNativeFunction("setChoice")({ id: "polisher", index });
          }
        }}
      >
        <option value={0}>Off</option>
        <option value={1}>On</option>
      </select>
      <span className="font-brand text-ink">MIX</span>
      <div
        className="relative h-[18px] flex-1"
        style={{ clipPath: "polygon(6px 0, 100% 0, 100% calc(100% - 6px), calc(100% - 6px) 100%, 0 100%, 0 6px)" }}
      >
        <div className="absolute inset-0 border border-[var(--nk-line)] bg-surface-high" />
        <div
          className="absolute inset-y-0 left-0 bg-gradient-to-r from-accent-dim to-accent"
          style={{ width: `${Math.round(mix * 100)}%` }}
        />
        <input
          type="range"
          min={0}
          max={1}
          step={0.01}
          value={mix}
          className="absolute inset-0 z-10 w-full cursor-pointer opacity-0"
          onChange={(e) => {
            const value = Number(e.target.value);
            useHostStore.getState().setMix(value);
            if (hasJuceBridge()) {
              void getNativeFunction("setParam")({ id: "dryWet", value, gesture: "change" });
            }
          }}
        />
        <div
          className="absolute top-[-3px] h-[24px] w-2.5 border border-[var(--nk-line)] bg-black"
          style={{ left: `calc(${mix * 100}% - 5px)` }}
        />
      </div>
      <span className="w-10 text-right text-ink">{Math.round(mix * 100)}%</span>
    </div>
  );
}

export function Footer() {
  const h = useHostStore();
  const cpu = Math.max(0, Math.min(100, Math.round(h.cpu)));
  const latMs = h.sr > 0 ? (h.lat / h.sr) * 1000 : 0;
  return (
    <footer className="grid h-[28px] shrink-0 grid-cols-10 items-center border-t border-[var(--nk-line)] px-2 font-mono text-[12px] text-muted">
      <span>NKOS</span>
      <span>{h.mode === "SAFE" ? "SAFE" : h.mode}</span>
      <span>CPU {String(cpu).padStart(3, " ")}%</span>
      <span>LAT {latMs > 0 ? `${latMs.toFixed(1)}ms/${h.lat}smp` : `${h.lat}smp`}</span>
      <span>SR {h.sr || "-"} 32f</span>
      <span>BUF {h.buf || "-"}</span>
      <span>BPM {h.bpm.toFixed(1)}</span>
      <span>{h.tempoSource}</span>
      <span>OS {h.osFactor}x</span>
      <span className="text-right">{footerLicenseLabel(h.licensed, h.demoRemainSec)}</span>
    </footer>
  );
}
