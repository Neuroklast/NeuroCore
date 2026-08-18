import { getNativeFunction, hasJuceBridge } from "../bridge/juce";
import { addCircuitBlock } from "../assemble/addBlock";
import { irSlotsFromScript, mergeIrSlots } from "../presets/irSlots";
import { useAstStore } from "../store/astStore";
import { useHostStore } from "../store/hostStore";

export function loadIrFile(slot: string): void {
  if (! hasJuceBridge()) {
    return;
  }
  void getNativeFunction("pickFile")({ kind: "ir", slot }).catch(() => undefined);
}

export function irSlotAction(action: "clear" | "preview", slot: string): void {
  if (! hasJuceBridge()) {
    return;
  }
  void getNativeFunction("irSlot")({ action, slot }).catch(() => undefined);
}

export function openImpulse(slot: string): void {
  useHostStore.getState().setOverlay("ir", slot);
}

export function ImpulsePanel({ focusSlot }: { focusSlot: string | null }) {
  const hostSlots = useHostStore((s) => s.irSlots);
  const script = useAstStore((s) => s.lastValidScript || s.script);
  const rows = mergeIrSlots(irSlotsFromScript(script), hostSlots);
  const focus = (focusSlot ?? "").trim().toLowerCase();

  if (rows.length === 0) {
    return (
      <div className="flex flex-col gap-3">
        <p>No cabinet in this chain. Add an IR chip, then load a WAV.</p>
        <button
          type="button"
          className="nk-clip self-start"
          onClick={() => {
            addCircuitBlock("ir");
          }}
        >
          Add Cabinet IR
        </button>
      </div>
    );
  }

  return (
    <ul>
      {rows.map((s) => (
        <li
          key={s.slot}
          className={`mb-3 flex flex-wrap items-center gap-2 ${
            s.slot === focus ? "border-l-2 border-accent pl-2" : ""
          }`}
        >
          <span className="min-w-[10rem]">
            {s.slot}: {s.loaded ? s.name : "empty"}
          </span>
          <button type="button" className="nk-clip" onClick={() => loadIrFile(s.slot)}>
            Load
          </button>
          <button
            type="button"
            className="nk-clip"
            disabled={! s.loaded}
            onClick={() => irSlotAction("preview", s.slot)}
          >
            Play
          </button>
          <button
            type="button"
            className="nk-clip"
            disabled={! s.loaded}
            onClick={() => irSlotAction("clear", s.slot)}
          >
            Clear
          </button>
        </li>
      ))}
    </ul>
  );
}
