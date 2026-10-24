import { useEffect, useState } from "react";
import { getNativeFunction, hasJuceBridge, onNativeEvent } from "../bridge/juce";
import { useHostStore } from "../store/hostStore";

/** Only rendered by the JUCE standalone application. DAWs retain their own transport. */
export function StandaloneTransportBar() {
  const standalone = useHostStore((s) => s.standalone);
  const playing = useHostStore((s) => s.transportPlaying);
  const ppq = useHostStore((s) => s.transportPpq);
  const bpm = useHostStore((s) => s.bpm);
  const source = useHostStore((s) => s.auditionSource);
  const file = useHostStore((s) => s.auditionFile);
  const [tempo, setTempo] = useState(String(Math.round(bpm)));
  const [status, setStatus] = useState("");
  useEffect(() => { setTempo(String(Math.round(bpm))); }, [bpm]);
  useEffect(() => onNativeEvent("standaloneAudioSelected", (payload) => {
    const value = (payload ?? {}) as { ok?: boolean; message?: string };
    setStatus(value.message || (value.ok ? "Audio loaded" : "Could not load audio."));
  }), []);

  if (!standalone) return null;
  const action = (command: string, value?: number) => {
    if (!hasJuceBridge()) return;
    setStatus("");
    void getNativeFunction("standaloneTransport")({ action: command, value })
      .catch((error: unknown) => setStatus(error instanceof Error ? error.message : "Transport command failed."));
  };
  const commitTempo = () => {
    const value = Number(tempo);
    if (Number.isFinite(value) && value >= 20 && value <= 400) action("bpm", value);
    else { setStatus("BPM must be 20–400."); setTempo(String(Math.round(bpm))); }
  };

  return (
    <div className="flex h-[40px] w-full shrink-0 items-center gap-2 overflow-hidden border-y border-[var(--nk-line)] bg-surface-high px-3 text-[12px]"
      role="group" aria-label="Standalone Transport">
      <span className="font-brand tracking-wider text-muted">TRANSPORT</span>
      <button type="button" className={"nk-clip " + (playing ? "on" : "")}
        aria-label={playing ? "Pause" : "Play"} aria-pressed={playing}
        onClick={() => action(playing ? "pause" : "play")}>{playing ? "Ⅱ PAUSE" : "▶ PLAY"}</button>
      <button type="button" className="nk-clip" aria-label="Stop" onClick={() => action("stop")}>■ STOP</button>
      <label className="flex shrink-0 items-center gap-1">
        <span className="text-muted">BPM</span>
        <input className="w-[64px] border border-[var(--nk-line)] bg-surface px-1 py-1 text-center text-ink"
          type="number" min="20" max="400" step="1" value={tempo}
          onChange={(e) => setTempo(e.target.value)} onBlur={commitTempo}
          onKeyDown={(e) => { if (e.key === "Enter") e.currentTarget.blur(); }}/>
      </label>
      <span className="text-muted">SOURCE</span>
      <select aria-label="Standalone audio source" className="max-w-[130px] border border-[var(--nk-line)] bg-surface px-1 py-1"
        value={source} onChange={(e) => action(e.target.value)}>
        <option value="input">Audio input</option>
        <option value="file">Audio file</option>
        <option value="tone">Test tone (440 Hz)</option>
      </select>
      <button className="nk-clip" type="button" onClick={() => {
        setStatus("");
        void getNativeFunction("pickFile")({ kind: "standaloneAudio" })
          .catch((error: unknown) => setStatus(error instanceof Error ? error.message : "Audio chooser failed."));
      }}>LOAD AUDIO…</button>
      <span className="min-w-0 flex-1 truncate text-muted" title={status || file}>
        {status || (source === "tone" ? "Test generator on · 440 Hz · low level" :
          source === "file" ? (file || "No audio loaded") : "External input · generator off")}
      </span>
      <span className="shrink-0 text-muted">{playing ? "RUNNING" : ppq > 1e-6 ? "PAUSED" : "STOPPED"}</span>
    </div>
  );
}
