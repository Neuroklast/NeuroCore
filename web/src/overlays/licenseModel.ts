export const LICENSE_ACTIVATE_URL = "https://neuroklast.net/api/license/activate";

export function licenseStatusLine(licensed: boolean, demoRemainSec: number): string {
  if (licensed) {
    return "LICENSED";
  }
  const s = Math.max(0, Math.floor(demoRemainSec));
  const m = Math.floor(s / 60);
  const r = s % 60;
  return `DEMO ${m}:${r.toString().padStart(2, "0")} until dry mute`;
}

export function licenseBuyerVisible(email: string): string {
  return email.trim();
}
