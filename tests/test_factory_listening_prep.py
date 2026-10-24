"""Contracts on the 2026-10-24 factory render snapshot and loudness floors."""
import json
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
from factory_listening_prep import is_send, loudness_floors  # noqa: E402

RENDER = ROOT / "docs" / "presets" / "2026-10-24-factory-render.json"
CATALOG = ROOT / "resources" / "factory_presets.json"
EXPECTED_QUIET = {
    "Ambient Swell Guitar",
    "MIDI Swell Return",
    "Vocal Presence Duck",
}


class FactoryListeningPrep(unittest.TestCase):
    def test_render_covers_catalog_with_zero_failures(self):
        data = json.loads(RENDER.read_text(encoding="utf-8"))
        catalog = json.loads(CATALOG.read_text(encoding="utf-8"))
        self.assertEqual(data["count"], 363)
        self.assertEqual(data["failures"], 0)
        names = [row["name"] for row in data["presets"]]
        self.assertEqual(len(names), 363)
        self.assertEqual(set(names), {p["name"] for p in catalog})
        self.assertTrue(all(int(row["nonfinite"]) == 0 for row in data["presets"]))
        self.assertTrue(all("error" not in row for row in data["presets"]))

    def test_loudness_queue_matches_native_quiet_inserts(self):
        data = json.loads(RENDER.read_text(encoding="utf-8"))
        catalog = {
            row["name"]: row.get("script") or ""
            for row in json.loads(CATALOG.read_text(encoding="utf-8"))
        }
        quiet = []
        for row in data["presets"]:
            if is_send(row["category"], row["name"]):
                continue
            peak_floor, rms_floor = loudness_floors(
                row["category"], catalog.get(row["name"], "")
            )
            if float(row["peak"]) < peak_floor or float(row["rms"]) < rms_floor:
                quiet.append(row["name"])
        self.assertEqual(set(quiet), EXPECTED_QUIET)


if __name__ == "__main__":
    unittest.main()
