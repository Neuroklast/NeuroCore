import { demoEndDateLabel } from "../chrome/footerLicense";

export const LICENSE_ACTIVATE_URL = "https://neuroklast.net/api/license/activate";

export function licenseStatusLine(licensed: boolean, demoRemainSec: number, nowMs = Date.now()): string {
  if (licensed) {
    return "LICENSED";
  }
  const s = Math.max(0, Math.floor(demoRemainSec));
  if (s <= 0) {
    return "DEMO ended — Mix stays dry until you install a license";
  }
  const days = Math.max(1, Math.ceil(s / 86400));
  return `Demo ends ${demoEndDateLabel(s, nowMs)} (${days} day${days === 1 ? "" : "s"} left)`;
}

export function licenseBuyerVisible(email: string): string {
  return email.trim();
}
