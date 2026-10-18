/** Which telemetry buffer a board node can honestly read. No invented taps. */

export type NodeProbe = "plugin.in" | "plugin.out" | "none";

export function nodeProbe(nodeId: string, role?: string, type?: string): NodeProbe {
  const id = nodeId.trim().toUpperCase();
  if (id === "IN" || type === "in" || role === "in") {
    return "plugin.in";
  }
  if (id === "OUT" || type === "out" || role === "out") {
    return "plugin.out";
  }
  return "none";
}

export function probeLabel(probe: NodeProbe): string {
  if (probe === "plugin.in") {
    return "Plugin input";
  }
  if (probe === "plugin.out") {
    return "Plugin output";
  }
  return "No waveform tap";
}
