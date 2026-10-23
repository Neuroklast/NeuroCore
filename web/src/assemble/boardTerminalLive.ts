/** Short activity pulse from observed MIDI events, independent of held controller values. */
export function midiPlateLit(lastEventAt: number, now: number): boolean {
  return Number.isFinite(lastEventAt) && now >= lastEventAt && now - lastEventAt < 150;
}

/** SC plate stays visible; live only when the host pin is on. */
export function sidechainPlateLive(sidechainOn: boolean): boolean {
  return sidechainOn;
}
