export function footerLicenseLabel(licensed: boolean, demoRemainSec: number): string {
  if (licensed) {
    return "LIC";
  }
  const s = Math.max(0, Math.round(demoRemainSec));
  return `DEMO ${Math.floor(s / 60)}:${String(s % 60).padStart(2, "0")}`;
}
