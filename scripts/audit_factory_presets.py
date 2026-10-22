"""Per-entry factory review from source, complete archives and native render evidence."""
import argparse
import collections
import hashlib
import json
import math
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument("--baseline", required=True, type=Path)
parser.add_argument("--before", required=True, type=Path)
parser.add_argument("--after", required=True, type=Path)
parser.add_argument("--extra", action="append", default=[], type=Path)
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
load = lambda p: json.loads(Path(p).read_text())
baseline = load(args.baseline)
active = load(root / "resources/factory_presets.json")
archived = load(root / "resources/deprecated/factory-2026-10-22.json")
manifest = {p["name"]: p for p in load(root / "resources/deprecated/factory-2026-10-22-manifest.json")}
before, after = load(args.before), load(args.after)
originals, current, backups = ({p["name"]: p for p in entries} for entries in [baseline, active, archived])
measurements = {p["name"]: p for p in after}
old_measurements = {p["name"]: p for p in before}
failures = []
for p in baseline:
    name = p["name"]
    if name not in current or p != current[name]:
        if backups.get(name) != p:
            failures.append(f"{name}: original object missing from deprecated")
if current.get("Mesa High Gain") != originals["Mesa High Gain"]:
    failures.append("Mesa High Gain changed")
for p in after:
    if "error" in p or p.get("nonfinite", 0):
        failures.append(f"{p['name']}: {p.get('error', 'nonfinite audio')}")
for extra in args.extra:
    for p in load(extra):
        if "error" in p or p.get("nonfinite", 0):
            failures.append(f"{extra.name}/{p['name']}: invalid audio")
if failures:
    raise SystemExit("\n".join(failures))

# A compact deterministic temporal fingerprint, not a perceptual listening score.
def fingerprint(p):
    values = p.get("signature", [])
    if not values:
        return []
    values = values[::max(1, len(values) // 256)][:256]
    mean = sum(values) / len(values)
    centered = [x - mean for x in values]
    norm = math.sqrt(sum(x*x for x in centered))
    return [x / max(1e-20, norm) for x in centered]

def nearest_table(entries):
    vectors = [(p["name"], fingerprint(p)) for p in entries if "signature" in p]
    result = {}
    for name, vec in vectors:
        best = (-1.0, None)
        for other, compare in vectors:
            if other == name:
                continue
            score = sum(a*b for a, b in zip(vec, compare))
            if score > best[0]:
                best = (score, other)
        result[name] = {"name": best[1], "correlation": round(best[0], 6)}
    return result

def metrics(p):
    return {k: p[k] for k in ["peak", "rms", "dc", "nonfinite"] if k in p}

def blocks(p):
    import re
    script = re.sub(r"#.*|//[^\n]*", "", p["script"])
    return dict(collections.Counter(re.findall(r"^\s*([A-Za-z_]+)\d*:", script, re.M)))

extra_measurements = {p.stem: {row["name"]: metrics(row) for row in load(p)} for p in args.extra}
old_nearest = nearest_table(before)
new_nearest = nearest_table(after)
rows = []
for original in baseline:
    name = original["name"]
    decision = "deprecated" if name not in current else "revised" if original != current[name] else "keep"
    if decision == "keep":
        types = blocks(original)
        if "osc" in types:
            rationale = "Retain the intended periodic motion; sequenced/random modulation would change its timing and contour."
        elif "env" in types:
            rationale = "Retain the continuous input-following response; trigger/ADSR gating would change the job."
        elif original["category"] in ["Mastering", "Mixing", "Dynamics"]:
            rationale = "Retain fixed corrective/level control; do not introduce unrequested rhythmic or random modulation."
        else:
            rationale = "Retain this " + original["category"].lower() + " role and its " + ", ".join(types) + " topology."
    else:
        rationale = manifest[name]["reason"]
    row = {
        "name": name, "category": original["category"], "decision": decision,
        "purpose": original["description"] if decision == "deprecated" else current[name]["description"],
        "rationale": rationale, "replacement": manifest.get(name, {}).get("replacement"),
        "blocksBefore": blocks(original),
        "blocksAfter": blocks(current[name]) if name in current else None,
        "baseline": metrics(old_measurements.get(name, {})),
        "current": metrics(measurements.get(name, {})),
        "nearestBaselineFingerprint": old_nearest.get(name),
        "nearestCurrentFingerprint": new_nearest.get(name),
        "protected": name == "Mesa High Gain",
    }
    rows.append(row)
for p in active:
    if p["name"] not in originals:
        rows.append({
            "name": p["name"], "category": p["category"], "decision": "new",
            "purpose": p["description"], "rationale": "Adds this explicit processing/control-source use case.",
            "blocksAfter": blocks(p), "current": metrics(measurements.get(p["name"], {})),
            "nearestCurrentFingerprint": new_nearest.get(p["name"]),
        })
for row in rows:
    row["additionalMeasurements"] = {profile: values.get(row["name"]) for profile, values in extra_measurements.items() if row["name"] in values}
report = {
    "base": "d07c0ba1c3dc", "version": "0.7.2-beta",
    "originalCount": len(baseline), "activeCount": len(active), "archivedVersions": len(archived),
    "decisions": dict(collections.Counter(r["decision"] for r in rows)),
    "method": "Every entry: source/macro/use-case review plus native 48 kHz stereo default render. Host tempo runs at 120 BPM; MIDI/sidechain routed when requested. Additional render profiles are listed separately. Fingerprint correlation only flags candidates, never proves perceptual equivalence.",
    "limitations": "Offline SignalChain measurements. No claim of human audition of every preset, LUFS/true-peak compliance or DAW acceptance. Static corrective tools can correctly sound alike on a limited test signal and must be distinguished by job and stimulus response.",
    "additionalProfiles": [p.name for p in args.extra],
    "presets": rows,
}
directory = root / "docs/presets"
(directory / "2026-10-22-review.json").write_text(json.dumps(report, ensure_ascii=False, indent=2)+"\n")
lines = [
    "# Individual factory preset review", "",
    f"Base d07c0ba1c3dc. {len(baseline)} original entries reviewed. {len(active)} active entries. Decisions: "+str(report["decisions"])+".",
    "", report["method"], "", report["limitations"], "",
    "Complete originals of all removed and revised presets are in resources/deprecated/factory-2026-10-22.json. Mesa High Gain is unchanged.", "",
    "| Preset | Category | Decision | Review and purpose |", "| --- | --- | --- | --- |",
]
for r in rows:
    text = (r["rationale"]+" "+r["purpose"]).replace("|", "/").replace("\n", " ")
    lines.append(f"| {r['name']} | {r['category']} | {r['decision']} | {text} |")
(directory / "2026-10-22-review.md").write_text("\n".join(lines)+"\n")
print(json.dumps({k: report[k] for k in ["originalCount", "activeCount", "archivedVersions", "decisions"]}))
