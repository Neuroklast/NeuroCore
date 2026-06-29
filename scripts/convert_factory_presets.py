#!/usr/bin/env python3
"""Convert factory_presets.json scripts from brace to line DSL syntax."""
import json
import re
from pathlib import Path

SHAPE_MAP = {
    "sin": "sine",
    "tri": "triangle",
    "saw": "saw",
    "square": "square",
    "noise": "noise",
}


def parse_kv_body(body: str) -> dict[str, str]:
    flat = [q if q else t for q, t in re.findall(r'"([^"]+)"|(\S+)', body)]
    kv: dict[str, str] = {}
    key = None
    for item in flat:
        if key is None:
            key = item.lower()
        else:
            kv[key] = item
            key = None
    return kv


def extract_blocks(script: str) -> list[tuple[str, str]]:
    script = script.replace("\r\n", "\n")
    blocks: list[tuple[str, str]] = []
    i = 0
    while i < len(script):
        if script[i] in " \t\n":
            i += 1
            continue
        m = re.match(r"(\w+)\s*\{", script[i:])
        if not m:
            i += 1
            continue
        btype = m.group(1).lower()
        start = i + m.end() - 1
        depth = 0
        j = start
        while j < len(script):
            if script[j] == "{":
                depth += 1
            elif script[j] == "}":
                depth -= 1
                if depth == 0:
                    blocks.append((btype, script[start + 1 : j].strip()))
                    i = j + 1
                    break
            j += 1
        else:
            break
    return blocks


def convert_script(script: str) -> str:
    lines_out: list[str] = []
    counters = {"stage": 0, "filter": 0, "comp": 0, "env": 0, "osc": 0}

    for btype, body in extract_blocks(script):
        if btype == "param":
            kv = parse_kv_body(body)
            alias = kv.get("alias", "a")
            name = kv.get("name", alias)
            mn = kv.get("min", "0")
            mx = kv.get("max", "1")
            lines_out.append(f"param {alias} = {name} [{mn}, {mx}]")

        elif btype == "stage":
            counters["stage"] += 1
            name = f"stage{counters['stage']}"
            compact = " ".join(body.split())
            if compact.startswith("y "):
                formula = compact[2:].strip()
            else:
                formula = compact
            lines_out.append(f"{name}: y = {formula}")

        elif btype == "filter":
            counters["filter"] += 1
            name = f"filter{counters['filter']}"
            kv = parse_kv_body(body)
            parts = [f"{k} = {v}" for k, v in kv.items()]
            lines_out.append(f"{name}: " + "; ".join(parts))

        elif btype == "osc":
            kv = parse_kv_body(body)
            osc_name = kv.get("name", f"osc{counters['osc'] + 1}")
            counters["osc"] += 1
            shape = SHAPE_MAP.get(kv.get("shape", "sine").lower(), kv.get("shape", "sine"))
            parts = [f"shape = {shape}"]
            if "freq" in kv:
                parts.append(f"freq = {kv['freq']}")
            if "sync" in kv:
                parts.append(f"sync = {kv['sync']}")
            if "depth" in kv:
                parts.append(f"depth = {kv['depth']}")
            lines_out.append(f"{osc_name}: " + "; ".join(parts))

        elif btype == "env":
            kv = parse_kv_body(body)
            env_name = kv.get("name", f"env{counters['env'] + 1}")
            counters["env"] += 1
            mode = kv.get("mode", "rms").lower()
            parts = [f"type = {mode}"]
            if "attack" in kv:
                parts.append(f"attack = {kv['attack']}")
            if "release" in kv:
                parts.append(f"release = {kv['release']}")
            if "trigger" in kv:
                parts.append(f"trigger = {kv['trigger']}")
            lines_out.append(f"{env_name}: " + "; ".join(parts))

        elif btype == "comp":
            counters["comp"] += 1
            name = f"comp{counters['comp']}"
            kv = parse_kv_body(body)
            parts = []
            for k in ("threshold", "ratio", "attack", "release"):
                if k in kv:
                    parts.append(f"{k} = {kv[k]}")
            lines_out.append(f"{name}: " + "; ".join(parts))

    return "\n".join(lines_out)


def main() -> None:
    path = Path(__file__).resolve().parents[1] / "resources" / "factory_presets.json"
    data = json.loads(path.read_text(encoding="utf-8"))
    for preset in data:
        preset["script"] = convert_script(preset["script"])
    path.write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"Converted {len(data)} presets -> {path}")


if __name__ == "__main__":
    main()