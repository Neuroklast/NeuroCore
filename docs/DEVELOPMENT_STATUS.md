# Entwicklungsstand NEUROKORE

**Stand:** 2026-08-16  
**Version:** 0.4.7-alpha  

Alte Tages-Checklisten: `docs/archive/DEVELOPMENT_STATUS_HISTORY.md`.

## Jetzt kaputt (Screenshots)

Quelle: `screenshots/Screenshot 2026-08-16 231501.png` (Phaser Lab), `231939.png` (Settings).

| Fläche | Ist | Soll (`docs/agents/ui-ux.md`) |
|---|---|---|
| Circuit tidy | Soft-Contract: `OUT(464,16)` sitzt in der Main-Zeile, während `filter3` bei x=1136 liegt (`CircuitContract` Dump). Screenshot 231501: Kabel umrunden das Board. | OUT rechts der letzten Chip-Spalte, unten |
| Knob | Letter im Chip-Titel (gut). Expand/Reihenfolge weiter falsch | Keine Kabel; Labels; Höhe = Jacks |
| Footer | Screenshot zeigt noch `CPU 173%` + SAFE | 0–100 via `cpuDisplayPercent`; SAFE ist nur das Mode-Wort |
| Settings | License / Help als Haarlinie | Trefferfläche ≥ 26 px, Overlay so hoch wie der Inhalt |

Circuit-Checklisten von heute als `[x]` zu führen war falsch. Die Verträge in `tests/CircuitContractTest.h` sind die Wahrheit.

## Offen, in dieser Reihenfolge

1. **Ein** `tidyLayout`-Modell (Ranks + Rails), bis Phaser Lab den IN/OUT-Vertrag erfüllt. Kein Preset-Sonderfall.
2. Settings-Overlay: letzte Zeile nicht zusammenschieben.
3. Soft-Contracts in CircuitContractTest von „log“ auf `expect` stellen, wenn 1. steht.

## Modul-Notiz (nur wo unsicher)

| Modul | Wahrheit |
|---|---|
| GraphModel / GraphCanvas / PcbRouter | Orthogonal-Router existiert. Auto-Arrange **erfüllt den Screenshot-Vertrag nicht**. |
| CpuProtect / Footer | Anzeige soll 0–100 sein. 8× + LFO-Filter kann den Guard trotzdem trippen. |
| Tests | Zu viele Sample-`expect`. Neue Arbeit = Contracts. |
| Rest | Unverändert nutzbar (DSL, Factory, License, OS-Bänke). |

## Build

```bash
cmake --build build --config Release --target NeuroKore_All
cmake --build build --target NeuroKoreTests --config Release
```
