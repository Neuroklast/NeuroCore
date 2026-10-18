"""Installer treats leftover WebView2 keys as installed unless pv is a real version."""
import re
from pathlib import Path

root = Path(__file__).resolve().parents[1]
iss = (root / "installer/NeuroKore.iss").read_text(encoding="utf-8")

fn = re.search(
    r"function WebView2Installed:\s*Boolean;\s*begin(?P<body>.*?)(?=^function |^procedure |\Z)",
    iss,
    re.S | re.M,
)
assert fn, "WebView2Installed missing"
body = fn.group("body")
assert "RegKeyExists" not in body, body
assert "WebView2PvPresent" in body, body

present = re.search(
    r"function WebView2PvPresent\([^)]*\):\s*Boolean;\s*(?:var.*?)\s*begin(?P<body>.*?)end;",
    iss,
    re.S,
)
assert present, iss
pbody = present.group("body")
assert "RegQueryStringValue" in pbody, pbody
assert "'pv'" in pbody, pbody
assert "0.0.0.0" in pbody, pbody
assert "RegKeyExists" not in pbody, pbody
assert "Version <> ''" in pbody, pbody

print("WebView2Installed reads a non-zero pv: PASS")
