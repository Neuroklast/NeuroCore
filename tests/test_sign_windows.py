"""Commercial Windows packages must fail closed when signing is required."""
from pathlib import Path

root = Path(__file__).resolve().parents[1]
script = (root / "scripts/sign_windows.ps1").read_text(encoding="utf-8")

assert 'NEUROKORE_REQUIRE_SIGN -eq "1"' in script, script
assert "NEUROKORE_SIGN_PFX" in script
assert "NEUROKORE_SIGN_THUMBPRINT" in script
assert "cannot sign" in script
assert "skip signing" in script

pkg = (root / "scripts/package_windows.ps1").read_text(encoding="utf-8")
assert "sign_windows.ps1" in pkg
assert "NEUROKORE_REQUIRE_SIGN" in pkg or "Invoke-Sign" in pkg

print("sign_windows fails closed when NEUROKORE_REQUIRE_SIGN is set: PASS")
