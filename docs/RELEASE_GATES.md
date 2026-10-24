# 0.8.0-beta release gates

Honest status for the remaining Bestandsprüfung items. Version is **0.8.0-beta**. This is not a 1.0 claim.

## Licensing

| Gate | Status |
|---|---|
| Ed25519 v2 device-bound license | Implemented. Empty fail-closes. Legacy RSA rejected. |
| Alpha keys | Allowed (`NEUROKORE_ALLOW_ALPHA_LICENSES=ON`). Stable/sale builds must configure `OFF`. |
| Demo mix cap | Engine `wetMixCap`. Host Mix parameter is not written. |
| Online activate | HTTP 200 is not a license; native `importLicense` must succeed. |
| Website / Stripe | Separate. Not in this plugin tree. |

Native: `license-contract.yml` on licensing paths. Tests skip product enforcement (`NEUROKORE_SKIP_LICENSE_ENFORCEMENT`).

## Platform matrix

| OS | Format | How it is built | This pass |
|---|---|---|---|
| Windows x64 | VST3 `NEUROKORE.vst3` | `build_release.bat` / `package_windows.ps1` | Local pluginval 1.0.4 strictness 5 and 10 SUCCESS (uniqueId `18b17b0d`) |
| Windows x64 | Standalone | Same kit | Packaged. Not a DAW host. |
| Windows x64 | Inno Setup | Same kit | Unsigned unless a cert env is set |
| macOS universal | VST3 + AU | `.github/workflows/native-alpha.yml` (`workflow_dispatch`) | Not run this pass. No Developer ID / notarize. |
| Linux x86_64 | tests + VST3 | `native-alpha.yml` | Not a sale SKU. Not run this pass. |

`native-alpha.yml` is the native platform matrix. It is manual. PR CI is web-layout (web paths) plus license-contract (licensing paths).

## Installer

- VST3 always lands in `Common Files\VST3\NEUROKORE.vst3` (stable name).
- Leftover `NEUROKORE-*.vst3` folders are deleted **only when the VST3 component is selected**.
- WebView2: registry `pv` must be a real version; missing runtime runs the bundled bootstrapper (needs network). Offline bootstrapper is still open.
- Contract: `tests/test_installer_vst3_cleanup.py`, `tests/test_webview2_runtime.py`.

## Signing

| Channel | Rule |
|---|---|
| Local / beta kit | `scripts/sign_windows.ps1` skips when cert or signtool is missing. |
| Sale kit | Set `NEUROKORE_REQUIRE_SIGN=1` plus `NEUROKORE_SIGN_PFX` (or `NEUROKORE_SIGN_THUMBPRINT`). Package aborts if unsigned. |
| macOS | Ad-hoc only in alpha scripts. Developer ID + notarize not implemented. No Apple cert in this environment. |

Contract: `tests/test_sign_windows.py`.

## DAW compatibility

| Host | Status |
|---|---|
| pluginval 1.0.4 Windows VST3 | Strictness 5 (3 repeats) and 10 SUCCESS on the 0.7.2-beta bundle |
| Cubase / FL Studio / Ableton / Reaper / Logic | Not certified this pass |
| AU / macOS scan | Not run |

pluginval is the automated host stand-in. A real DAW session (MIDI, sidechain, UI, save/restore) is still a human pass.

## Still blocked here

- Authenticode / Developer ID certificates
- macOS / Linux package build on this machine
- Human listening of factory presets in a DAW (MIDI-gated and sidechain inserts especially)
- UI nkperf numbers
