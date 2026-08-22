const MONTHS = ["Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"];

export function demoEndDateLabel(remainSec: number, nowMs = Date.now()): string {
  const s = Math.max(0, Math.round(remainSec));
  const end = new Date(nowMs + s * 1000);
  return `${end.getUTCDate()} ${MONTHS[end.getUTCMonth()]} ${end.getUTCFullYear()}`;
}

export function footerLicenseLabel(licensed: boolean, demoRemainSec: number, nowMs = Date.now()): string {
  if (licensed) {
    return "LIC";
  }
  const s = Math.max(0, Math.round(demoRemainSec));
  if (s <= 0) {
    return "DEMO ended";
  }
  const days = Math.max(1, Math.ceil(s / 86400));
  return `DEMO ${days}d · ${demoEndDateLabel(s, nowMs)}`;
}
