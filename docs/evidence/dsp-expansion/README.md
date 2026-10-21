# DSP Expansion — Evidence (PHASE 0)

Dieser Ordner dokumentiert den Istzustand vor der NEUROKORE-DSP-Expansion und dient als Referenz für alle späteren Änderungen.

## Dateien

| Datei | Inhalt |
|---|---|
| `00-baseline.md` | Baseline: Git-Commit/Branch, Kompilierstatus, Teststatus (32041 passed / 5 failed, alle vorbestehend), CPU/Speicher/Latenz, Presetwechsel/Zustandswiederherstellung. |
| `01-inventory-matrix.json` | Maschinenlesbare Bestandsmatrix aller Blöcke (siehe Felder unten). |

## Bestandsmatrix — Felder

Jeder Block-Eintrag enthält:

- `blockId` — stabile Block-ID
- `label`, `category` — Anzeigename / Kategorie
- `nativeParser` — wird vom `DSLParser` akzeptiert (Whitelist in `src/dsl/DSLParser.cpp`)
- `nativeDSP` — native Verarbeitung existiert (`src/dsl/SignalChain.cpp` + `src/dsl/blocks/*.cpp`)
- `circuitSpec` — Eintrag in `web/src/assemble/chipSpec.ts`
- `addMenu` — im Circuit-Hinzufügen-Menü (`web/src/assemble/addBlock.ts`, `ADDABLE_BLOCKS`)
- `terminalCompletion` — Terminal-Autocomplete kennt den Block (`web/src/hack/dslComplete.ts`, `BLOCKS`)
- `inspector` — Face/Inspector-Darstellung (`web/src/assemble/nodeFace/*`)
- `tests` — eigene Native-/Web-Tests existieren
- `documentation` — in `docs/DSL_REFERENCE.md` oder `docs/manual/NEUROKORE.md` beschrieben
- `status` — einer von:
  - `vollstaendig` — alle Felder wahr
  - `nicht_erreichbar` — native DSP + chipSpec existieren, aber nicht im Add-Menü / nicht erreichbar (z. B. OTT, Vocoder)
  - `teilweise` — implementiert, aber Lücken (z. B. EQ ohne `notch`, Gate ohne hold/hyst/range im chipSpec, Meter ohne UI)
  - `engine_ohne_ui` — native Engine existiert, UI fehlt komplett (Meter/Probe)
  - `nicht_implementiert` — noch gar nicht vorhanden (keine neuen Blöcke in Phase 0)
- `notes` — ergänzende Bemerkung

## Abgrenzung (Prinzip 2 aus dem Auftrag)

Bestehende Funktionen gelten **nicht** als fehlend, nur weil sie im Circuit-Menü nicht angezeigt werden. OTT und Vocoder haben eine vollständige native Engine; sie sind lediglich nicht erreichbar (`nicht_erreichbar`) und werden in Phase 1.3 ins Menü und den Inspector gebracht.

## Ausstehende Plattformtests

Native DAW-Validierung (pluginval, VST3-Host, Standalone, macOS AU), Preset-/DAW-Projekt-Restore und CPU-Messungen im Host sind `NOT RUN` (keine konfigurierte native DAW-Build-Umgebung für Host-Tests; nicht erfunden).
