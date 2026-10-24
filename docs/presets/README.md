# Factory preset curation, 2026-10-22

Base: main after #326 and #327, d07c0ba1c3dc. The initial inventory records every one of the 340 factory presets, including its intended use, actual block topology, exposed macro controls and original content hash. Mesa High Gain is protected.

This first checkpoint is an inventory, not a listening verdict. Native offline rendering and similarity measurements follow. Any removed entry must survive byte-for-byte as a complete deprecated preset object. Existing static mixing/mastering tools will not receive decorative modulation merely to use a new block. New modules are used when they improve the actual job.


The completed first pass is in [the individual review](2026-10-22-review.md), with machine-readable source/render decisions in the matching JSON. Measurements use the production SignalChain at 48 kHz, stereo, 256-sample blocks, with correct transport and requested MIDI/sidechain input. Correlation is a candidate detector, not a listening verdict. Similar linear responses on a limited test signal do not justify deleting a compressor, sidechain tool or stereo utility with a different job.

Reproduce native reports with NeuroKoreDspContracts --factory, --factory-profile (transients/hot/silence) and --factory-stress. Pass the JSON outputs to scripts/audit_factory_presets.py. The baseline catalog is main d07c0ba1c3dc. The protected Mesa baseline and all deprecated originals are checked by node --test scripts/factory/curation.test.mjs.

Final catalog: 353 active (25 new), including three initial MSEG examples. The final engine re-rendered every entry on five profiles with zero parse/nonfinite failures; 33 complete original versions remain archived. Source and decision counts in the individual review are authoritative. Windows/pluginval is tracked separately in the validation evidence.

The 2026-10-23 library pass starts from this catalog. Inventory and use-case classification: [2026-10-23-review.md](2026-10-23-review.md) and [2026-10-23-inventory.json](2026-10-23-inventory.json). Result report: [2026-10-23-report.md](2026-10-23-report.md).

2026-10-24 offline render of all 363 presets: [factory-render JSON](2026-10-24-factory-render.json) and [listening prep](2026-10-24-listening-prep.md). Zero apply/nonfinite failures. Human DAW listening, latency and LUFS remain open.

```
.\build\NeuroKoreTests_artefacts\Release\NeuroKoreTests.exe --factory-report docs/presets/2026-10-24-factory-render.json
py -3 scripts/factory_listening_prep.py docs/presets/2026-10-24-factory-render.json -o docs/presets/2026-10-24-listening-prep.md
```
