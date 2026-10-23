/** MIDI plate lamp from host mapper atomics, never from a graph edge. */
export function midiPlateLit(midiGate: number, midiVel = 0, midiMod = 0): boolean {
  return midiGate > 0.5 || midiVel > 0.001 || Math.abs(midiMod) > 0.001;
}

/** SC plate stays visible; live only when the host pin is on. */
export function sidechainPlateLive(sidechainOn: boolean): boolean {
  return sidechainOn;
}
