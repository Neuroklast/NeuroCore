"""Contracts on the 2026-10-24 factory render snapshot and loudness floors."""
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
from factory_listening_prep import is_send, loudness_floors  # noqa: E402

RENDER = ROOT / "docs" / "presets" / "2026-10-24-factory-render.json"
SHEET = ROOT / "docs" / "presets" / "2026-10-24-listening-prep.md"
CATALOG = ROOT / "resources" / "factory_presets.json"
EXPECTED_QUIET = set()


class FactoryListeningPrep(unittest.TestCase):
    def test_render_covers_catalog_with_zero_failures(self):
        data = json.loads(RENDER.read_text(encoding="utf-8"))
        catalog = json.loads(CATALOG.read_text(encoding="utf-8"))
        self.assertEqual(data["count"], len(catalog))
        self.assertEqual(data["failures"], 0)
        names = [row["name"] for row in data["presets"]]
        self.assertEqual(len(names), len(catalog))
        self.assertEqual(set(names), {p["name"] for p in catalog})
        self.assertTrue(all(int(row["nonfinite"]) == 0 for row in data["presets"]))
        self.assertTrue(all("error" not in row for row in data["presets"]))

    def test_midi_floors_include_gate_and_required_inputs(self):
        self.assertEqual(
            loudness_floors("Ambient", "adsr1: gate = midi\n", []),
            (0.02, 0.006),
        )
        self.assertEqual(
            loudness_floors("Ambient", "mseg1: source = midi\n", ["midi"]),
            (0.02, 0.006),
        )

    def test_loudness_queue_matches_native_quiet_inserts(self):
        data = json.loads(RENDER.read_text(encoding="utf-8"))
        catalog = {row["name"]: row for row in json.loads(CATALOG.read_text(encoding="utf-8"))}
        quiet = []
        for row in data["presets"]:
            if is_send(row["category"], row["name"]):
                continue
            entry = catalog.get(row["name"]) or {}
            peak_floor, rms_floor = loudness_floors(
                row["category"],
                entry.get("script") or "",
                entry.get("requiredInputs") or [],
            )
            if float(row["peak"]) < peak_floor or float(row["rms"]) < rms_floor:
                quiet.append(row["name"])
        self.assertEqual(set(quiet), EXPECTED_QUIET)

    def test_committed_markdown_matches_generator(self):
        with tempfile.TemporaryDirectory() as td:
            out = Path(td) / "listening.md"
            subprocess.check_call(
                [
                    sys.executable,
                    str(ROOT / "scripts" / "factory_listening_prep.py"),
                    str(RENDER),
                    "-o",
                    str(out),
                    "--catalog",
                    str(CATALOG),
                ],
                cwd=str(ROOT),
            )
            self.assertEqual(out.read_text(encoding="utf-8"), SHEET.read_text(encoding="utf-8"))


if __name__ == "__main__":
    unittest.main()
