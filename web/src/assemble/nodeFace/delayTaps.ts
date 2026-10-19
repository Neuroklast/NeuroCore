import { parseNoteToken, wholeToMs } from "../../chrome/noteValue";
import { faceBpm, parseFaceNumber, parseFaceToken, resolveFaceScalar, type FaceHost } from "./faceArgs";

/** Engine `Delay::kMaxDelaySec`. Sync notes use FaceHost bpm (else 120). */
export const DELAY_TIME_MIN = 0.1;
export const DELAY_TIME_MAX = 2000;
export const DELAY_FB_MAX = 0.95;
export const DELAY_TIME_AXIS = 2000;
const VIEW_W = 100;
const VIEW_H = 40;
const MIN_TAP = 0.02;

export type DelayFaceModel = {
  timeMs: number;
  feedback: number;
  mix: number;
  synced: boolean;
  pingpong: boolean;
};

function isSyncOff(raw: string): boolean {
  const t = raw.trim().toLowerCase();
  return ! t || t === "off" || t === "0" || t === "false" || t === "none";
}

function msFromToken(raw: string | undefined, fallback: number, host?: FaceHost): number {
  const t = (raw ?? "").trim();
  if (! t) {
    return fallback;
  }
  const n = resolveFaceScalar(t, host);
  if (n != null) {
    return Math.min(DELAY_TIME_MAX, Math.max(DELAY_TIME_MIN, n));
  }
  const whole = parseNoteToken(t);
  if (whole != null) {
    return Math.min(DELAY_TIME_MAX, Math.max(DELAY_TIME_MIN, wholeToMs(whole, faceBpm(host))));
  }
  return fallback;
}

export function delayFaceModel(args: Record<string, string>, host?: FaceHost): DelayFaceModel {
  const syncRaw = parseFaceToken(args.sync, "off");
  const synced = ! isSyncOff(syncRaw);
  const timeMs = synced ? msFromToken(args.sync, 250, host) : msFromToken(args.time ?? args.time_ms, 250, host);
  return {
    timeMs,
    feedback: parseFaceNumber(args.feedback, 0.25, 0, DELAY_FB_MAX, host),
    mix: parseFaceNumber(args.mix, 0.3, 0, 1, host),
    synced,
    pingpong: parseFaceToken(args.pingpong, "off") === "on" || parseFaceToken(args.pingpong, "off") === "true",
  };
}

export type DelayTap = { t: number; amp: number };

/** Dry at 0, then wet · fb^(n−1) at n · time. Matches Delay::process write/read. */
export function delayTaps(args: Record<string, string>, host?: FaceHost): DelayTap[] {
  const m = delayFaceModel(args, host);
  const taps: DelayTap[] = [{ t: 0, amp: 1 - m.mix }];
  if (m.mix <= 0 || m.timeMs <= 0) {
    return taps;
  }
  for (let n = 1; n * m.timeMs <= DELAY_TIME_AXIS; n += 1) {
    const amp = m.mix * (m.feedback ** (n - 1));
    if (amp < MIN_TAP && n > 1) {
      break;
    }
    taps.push({ t: n * m.timeMs, amp });
  }
  return taps;
}

export function delayTapsPath(args: Record<string, string>, host?: FaceHost): string {
  const taps = delayTaps(args, host);
  return taps.map((tap, i) => {
    const x = (VIEW_W * tap.t) / DELAY_TIME_AXIS;
    const y = VIEW_H - VIEW_H * Math.min(1, Math.max(0, tap.amp));
    return `${i === 0 ? "M" : "L"}${x.toFixed(2)} ${y.toFixed(2)}`;
  }).join(" ");
}

export function delayStemLines(args: Record<string, string>, host?: FaceHost): Array<{ x: number; y1: number }> {
  return delayTaps(args, host).map((tap) => ({
    x: (VIEW_W * tap.t) / DELAY_TIME_AXIS,
    y1: VIEW_H - VIEW_H * Math.min(1, Math.max(0, tap.amp)),
  }));
}
