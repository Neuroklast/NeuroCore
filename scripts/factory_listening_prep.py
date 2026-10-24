#!/usr/bin/env python3
"""Turn a --factory-report JSON into a listening-prep markdown sheet.

This is an offline peak/rms ranking, not a DAW or ear verdict.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path

INSERT_PEAK = 0.12
INSERT_RMS = 0.038

SEND_CATEGORIES = {"Delay", "Reverb"}
SEND_NAMES = {
    "Side Delay",
    "Side Hall",
    "Width Delay",
    "Slap Double",
}

LISTENING_NOTES = {
    "Ambient Swell Guitar": "Quiet insert on the sine+pink probe. Ear-check swell/gate vs a real guitar.",
    "MIDI Swell Return": "Quiet insert. Needs MIDI gate; confirm swell return in a DAW with notes.",
    "Vocal Presence Duck": "Zero output without sidechain. Confirm presence-band duck with a vocal on SC.",
}


def is_send(category: str, name: str) -> bool:
    if category in SEND_CATEGORIES:
        return True
    if "Send" in name:
        return True
    return name in SEND_NAMES


def loudness_floors(category: str, script: str) -> tuple[float, float]:
    """Same floors as tests/FactoryLoudnessTest.h."""
    s = script.lower()
    midi_gated = "source = midi" in s
    rhythmic = "stepseq1:" in s
    narrow_band = "type = bandpass" in s
    lo_fi = category.casefold() == "lo-fi"
    filt = category.casefold() == "filter"
    if midi_gated:
        return 0.02, 0.008
    if rhythmic:
        return 0.07, 0.025
    if narrow_band:
        return 0.09, 0.030
    if lo_fi:
        return 0.11, 0.035
    if filt:
        return 0.08, 0.025
    return INSERT_PEAK, INSERT_RMS


def main() -> int:
    p = argparse.ArgumentParser()
    p.add_argument("json_path", type=Path)
    p.add_argument("-o", "--out", type=Path, required=True)
    p.add_argument(
        "--catalog",
        type=Path,
        default=Path("resources/factory_presets.json"),
        help="Factory catalog used to match FactoryLoudness floors",
    )
    args = p.parse_args()

    data = json.loads(args.json_path.read_text(encoding="utf-8"))
    catalog = {
        row["name"]: row.get("script") or ""
        for row in json.loads(args.catalog.read_text(encoding="utf-8"))
    }
    presets = data["presets"]
    failures = int(data.get("failures", 0))
    apply_err = [r for r in presets if r.get("error")]
    nonfinite = [r for r in presets if int(r.get("nonfinite", 0)) != 0]

    ranked = sorted(presets, key=lambda r: float(r.get("peak") or 0.0), reverse=True)
    quietest = list(reversed(ranked[-15:]))
    hottest = ranked[:15]

    quiet_inserts = []
    for r in presets:
        if r.get("error") or int(r.get("nonfinite", 0)) != 0:
            continue
        if is_send(r.get("category") or "", r.get("name") or ""):
            continue
        peak = float(r.get("peak") or 0.0)
        rms = float(r.get("rms") or 0.0)
        peak_floor, rms_floor = loudness_floors(
            r.get("category") or "", catalog.get(r.get("name") or "", "")
        )
        if peak < peak_floor or rms < rms_floor:
            quiet_inserts.append(r)

    cats: dict[str, int] = {}
    for r in presets:
        cats[r.get("category") or "?"] = cats.get(r.get("category") or "?", 0) + 1

    lines: list[str] = []
    lines.append("# Factory listening prep, 2026-10-24")
    lines.append("")
    lines.append(
        "Machine render of every factory preset through `NeuroKoreAudioProcessor` "
        "(48 kHz, 512-sample blocks, 40 blocks, skip first 16). Probe is 440 Hz sine "
        "+ pink, no MIDI, no sidechain. This sheet ranks what to listen to. It is "
        "not a DAW, LUFS, or ear sign-off."
    )
    lines.append("")
    lines.append("Regenerate:")
    lines.append("")
    lines.append("```")
    lines.append(
        r".\build\NeuroKoreTests_artefacts\Release\NeuroKoreTests.exe "
        r"--factory-report docs/presets/2026-10-24-factory-render.json"
    )
    lines.append(
        "py -3 scripts/factory_listening_prep.py "
        "docs/presets/2026-10-24-factory-render.json "
        "-o docs/presets/2026-10-24-listening-prep.md"
    )
    lines.append("```")
    lines.append("")
    lines.append("## Totals")
    lines.append("")
    lines.append("| Metric | Value |")
    lines.append("|---|---|")
    lines.append(f"| Presets rendered | {data.get('count')} |")
    lines.append(f"| Apply / nonfinite failures | {failures} |")
    lines.append(f"| Apply errors | {len(apply_err)} |")
    lines.append(f"| Nonfinite output | {len(nonfinite)} |")
    lines.append(f"| Sample rate | {data.get('sampleRate')} |")
    lines.append(f"| Block / measured blocks | {data.get('block')} / {int(data.get('blocks', 0)) - 16} |")
    lines.append(f"| Quiet inserts vs FactoryLoudness floors | {len(quiet_inserts)} |")
    lines.append("")
    lines.append("## Ear-pass queue")
    lines.append("")
    lines.append(
        "Start here. Factory scripts are unchanged; retune only after a human pass."
    )
    lines.append("")
    lines.append("| Preset | Peak | RMS | Why |")
    lines.append("|---|---|---|---|")
    for r in quiet_inserts:
        name = r["name"]
        note = LISTENING_NOTES.get(name, "Below insert loudness floor on this probe.")
        lines.append(
            f"| {name} | {float(r['peak']):.3f} | {float(r['rms']):.4f} | {note} |"
        )
    lines.append("")
    lines.append("## Hottest 15 (peak)")
    lines.append("")
    lines.append("| Preset | Category | Peak | RMS |")
    lines.append("|---|---|---|---|")
    for r in hottest:
        lines.append(
            f"| {r['name']} | {r['category']} | {float(r['peak']):.3f} | {float(r['rms']):.4f} |"
        )
    lines.append("")
    lines.append("## Quietest 15 (peak)")
    lines.append("")
    lines.append("| Preset | Category | Peak | RMS |")
    lines.append("|---|---|---|---|")
    for r in quietest:
        lines.append(
            f"| {r['name']} | {r['category']} | {float(r['peak']):.3f} | {float(r['rms']):.4f} |"
        )
    lines.append("")
    lines.append("## Categories")
    lines.append("")
    lines.append("| Category | Count |")
    lines.append("|---|---|")
    for cat, n in sorted(cats.items(), key=lambda kv: (-kv[1], kv[0])):
        lines.append(f"| {cat} | {n} |")
    lines.append("")
    lines.append("## Limits")
    lines.append("")
    lines.append(
        "- MIDI-gated and sidechain presets are under-driven by this probe. "
        "`FactoryUseCaseTest` still fails Vocal Presence Duck (open=0 / duck=0) "
        "because the presence-band duck needs a vocal-like SC, not a 3.2 kHz tone "
        "on the main input alone."
    )
    lines.append(
        "- `FactoryLoudnessTest` uses these same floors. The ear-pass queue is that "
        "quiet-insert list. Sends (Delay/Reverb and named send-likes) are excluded."
    )
    lines.append(
        "- Subjective listening, latency, LUFS, and DAW load remain open."
    )
    lines.append("")

    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text("\n".join(lines), encoding="utf-8")
    print(f"wrote {args.out} ({len(presets)} presets, {failures} failures)")
    return 0 if failures == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
