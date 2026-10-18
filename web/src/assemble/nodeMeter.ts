import { nodeProbe } from "./nodeProbe";

export type NodeMeterSource = "plugin.in" | "plugin.out" | "node" | "none";
export type NodeWave = "plugin.in" | "plugin.out" | "none";

export type NodeMeter = {
  source: NodeMeterSource;
  sourceLabel: string;
  peak: number | null;
  rms: number | null;
  wave: NodeWave;
  waveLabel: string;
};

export function meterLabel(source: NodeMeterSource): string {
  if (source === "plugin.in") {
    return "Plugin input";
  }
  if (source === "plugin.out") {
    return "Plugin output";
  }
  if (source === "node") {
    return "Node peak";
  }
  return "No tap";
}

export function waveMeterLabel(wave: NodeWave): string {
  if (wave === "plugin.in") {
    return "Plugin input";
  }
  if (wave === "plugin.out") {
    return "Plugin output";
  }
  return "No waveform tap";
}

function finiteNum(raw: unknown): number | null {
  const n = Number(raw);
  return Number.isFinite(n) ? n : null;
}

function clipAt(clips: Record<string, number> | undefined, keys: readonly string[]): number | null {
  if (! clips) {
    return null;
  }
  for (const key of keys) {
    if (Object.prototype.hasOwnProperty.call(clips, key)) {
      return finiteNum(clips[key]);
    }
  }
  return null;
}

const EMPTY: NodeMeter = {
  source: "none",
  sourceLabel: meterLabel("none"),
  peak: null,
  rms: null,
  wave: "none",
  waveLabel: waveMeterLabel("none"),
};

/** Peak/RMS from that node's own tap. Never plugin.out under a DSP name. */
export function nodeMeter(input: {
  id: string;
  role?: string;
  type?: string;
  clips?: Record<string, number>;
  clipsRms?: Record<string, number>;
  pluginInPeak?: number;
  pluginOutPeak?: number;
  pluginInRms?: number;
  pluginOutRms?: number;
}): NodeMeter {
  const id = input.id.trim();
  if (! id) {
    return EMPTY;
  }
  const probe = nodeProbe(id, input.role, input.type);
  if (probe === "plugin.in") {
    const peak = clipAt(input.clips, [id, "IN", "__in__"]) ?? finiteNum(input.pluginInPeak);
    const rms = clipAt(input.clipsRms, [id, "IN", "__in__"]) ?? finiteNum(input.pluginInRms);
    return {
      source: "plugin.in",
      sourceLabel: meterLabel("plugin.in"),
      peak,
      rms,
      wave: "plugin.in",
      waveLabel: waveMeterLabel("plugin.in"),
    };
  }
  if (probe === "plugin.out") {
    const peak = clipAt(input.clips, [id, "OUT", "__out__"]) ?? finiteNum(input.pluginOutPeak);
    const rms = clipAt(input.clipsRms, [id, "OUT", "__out__"]) ?? finiteNum(input.pluginOutRms);
    return {
      source: "plugin.out",
      sourceLabel: meterLabel("plugin.out"),
      peak,
      rms,
      wave: "plugin.out",
      waveLabel: waveMeterLabel("plugin.out"),
    };
  }
  const peak = clipAt(input.clips, [id]);
  const rms = clipAt(input.clipsRms, [id]);
  const source: NodeMeterSource = peak != null ? "node" : "none";
  return {
    source,
    sourceLabel: meterLabel(source),
    peak,
    rms,
    wave: "none",
    waveLabel: waveMeterLabel("none"),
  };
}
