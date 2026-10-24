"""Standalone-only setup must not delete leftover versioned VST3 bundles."""
import re
from pathlib import Path

root = Path(__file__).resolve().parents[1]
iss = (root / "installer/NeuroKore.iss").read_text(encoding="utf-8")

cur = re.search(
    r"procedure CurStepChanged\(CurStep: TSetupStep\);\s*begin(?P<body>.*?)end;",
    iss,
    re.S,
)
assert cur, "CurStepChanged missing"
body = cur.group("body")
assert "RemoveOldVersionedBundles" in body, body
assert "WizardIsComponentSelected('vst3')" in body, body
assert "ssInstall" in body, body

remove = re.search(
    r"procedure RemoveOldVersionedBundles;\s*(?:var.*?)\s*begin(?P<body>.*?)end;",
    iss,
    re.S,
)
assert remove, "RemoveOldVersionedBundles missing"
rbody = remove.group("body")
assert "NEUROKORE-*.vst3" in rbody, rbody
assert "NEUROKORE.vst3" not in rbody.replace("NEUROKORE-*.vst3", ""), rbody

print("Old versioned VST3 bundles are removed only when the VST3 component is selected: PASS")
