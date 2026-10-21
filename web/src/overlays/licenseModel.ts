import { demoEndDateLabel } from "../chrome/footerLicense";

export const LICENSE_ACTIVATE_URL = "https://neuroklast.net/api/license/v1/activate";
export const LICENSE_DOCUMENT_MARK = "NEUROKORE LICENSE";

export type LicenseDocumentParse =
  | { ok: true; text: string }
  | { ok: false; kind: "http" | "no-document" };

export type ActivateOutcome = { ok: boolean; message: string };

function licenseField(value: unknown): string | null {
  return typeof value === "string" && value.includes(LICENSE_DOCUMENT_MARK) ? value : null;
}

export function licenseDocumentFromActivateResponse(status: number, body: string): LicenseDocumentParse {
  if (status < 200 || status >= 300) {
    return { ok: false, kind: "http" };
  }
  const trimmed = body.trim();
  if (!trimmed) {
    return { ok: false, kind: "no-document" };
  }
  try {
    const parsed: unknown = JSON.parse(trimmed);
    if (parsed && typeof parsed === "object") {
      const rec = parsed as Record<string, unknown>;
      const fromField = licenseField(rec.license) ?? licenseField(rec.lic)
        ?? licenseField(rec.text) ?? licenseField(rec.body);
      if (fromField) {
        return { ok: true, text: fromField };
      }
    }
  } catch {
    /* raw document */
  }
  if (trimmed.includes(LICENSE_DOCUMENT_MARK)) {
    return { ok: true, text: trimmed };
  }
  return { ok: false, kind: "no-document" };
}

export async function completeOnlineActivate(
  status: number,
  body: string,
  importLicense: (text: string) => Promise<boolean>,
): Promise<ActivateOutcome> {
  const parsed = licenseDocumentFromActivateResponse(status, body);
  if (!parsed.ok) {
    return {
      ok: false,
      message: parsed.kind === "http"
        ? "Online activate failed — use INSTALL .LIC"
        : "No license in the reply — use INSTALL .LIC",
    };
  }
  const stored = await importLicense(parsed.text);
  if (!stored) {
    return { ok: false, message: "License rejected — use INSTALL .LIC" };
  }
  return { ok: true, message: "License installed." };
}

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
