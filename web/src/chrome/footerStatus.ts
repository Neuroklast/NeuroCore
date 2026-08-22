export function footerSr(sr: number): string {
  return sr > 0 ? String(Math.round(sr)) : "—";
}

export function footerBuf(buf: number): string {
  return buf > 0 ? String(Math.round(buf)) : "—";
}
