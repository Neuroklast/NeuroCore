# Lessons Learned

Dieser Erfahrungsspeicher wird nach **jeder** Coding-Agent-Session ergänzt.
Er dient dazu, Fehler nicht zu wiederholen und bekannte Fallstricke zu dokumentieren.

---

## Session-Log

---

### 2026-04-01 – Initiale Analyse & Dokumentation

**Agent:** GitHub Copilot Coding Agent  
**Aufgabe:** Vollständige Codebase-Analyse und Dokumentationsstruktur erstellen  
**Ergebnis:** ✅ Erfolgreich  
**Commit:** `b5e76bb` (Analyse-Basis)

#### Erkenntnisse

**Codebase:**
- `PluginProcessor.cpp` ist mit ~34 KB eine massive God-Class, die dringend in kleinere, fokussierte Klassen aufgeteilt werden muss. Das ist das größte Architektur-Problem des Projekts.
- `ExpressionEvaluator` ist das Herzstück des Plugins – die Kombination aus SIMD, Constant-Folding und CSE-Elimination ist beeindruckend professionell implementiert.
- Das Licensing-System ist komplett nicht-funktional: `licensing.example.com` ist ein Placeholder. `kEnableLicensing = true` muss für alle Dev-Builds auf `false` gesetzt werden.
- Der `DSLParser` hat **keine Tests** – das ist die höchste Priorität für die nächste Entwicklungs-Session.
- Thread-Safety zwischen UI-Thread und Audio-Thread ist das größte Stabilitätsrisiko: `getScript()`, `shared_ptr<Chain>` und `SpinLock + juce::String` sind alle potentielle Race Conditions oder Heap-Allokationen im Audio-Thread.
- Die DSL-Idee ("ShaderToy für Audio") ist das absolute Alleinstellungsmerkmal – das muss das Herzstück aller Marketingbemühungen sein.
- Das `WeightedLayout`-System ist clever und flexibel, aber die feste Fenstergröße 1600×900 macht das Plugin auf kleineren Bildschirmen (z. B. 13" Laptops) schwierig nutzbar.
- Blowfish-Verschlüsselung für Presets ist ungewöhnlich – ein Upgrade auf AES-256 wäre professioneller.
- `autoGainCompensate()` mit per-Sample `getSubBlock()` ist ein ernsthafter Performance-Bottleneck.

**Build-System:**
- JUCE muss in Version ≥ 8.0.6 vorhanden sein. Die CMake-Integration lädt JUCE automatisch wenn `JUCE_DIR` nicht gesetzt ist.
- Die VST3-SDK muss manuell in `~/JUCE/modules/juce_audio_processors/format_types/VST3_SDK` kopiert werden.
- `CMakeLists.txt` hat eine doppelte Source-Einbindung (`SOURCES` in `juce_add_plugin` UND `target_sources`) – das ist ein Bug der Warnungen erzeugen kann.
- `SignalChainTest.h` existiert im `tests/`-Verzeichnis, ist aber nicht im `NeuroCoreTests` CMake-Target verlinkt.

#### Fallstricke

- Kein blindes Hinzufügen von Code in `PluginProcessor` – diese Klasse ist bereits zu groß
- Bei jeder neuen Klasse: Thread-Safety von Anfang an bedenken (Audio-Thread vs. Message-Thread)
- `juce::String`-Operationen unter Lock = potentielle Heap-Allokation = Audio-Thread-Knackser
- `std::shared_ptr` ist nicht thread-safe ohne explizite Synchronisation (`atomic_load`/`atomic_store`)

#### Empfehlungen für nächste Session

1. Als erstes `kEnableLicensing = false` setzen (verhindert Demo-Modus bei Entwicklung)
2. Dann Thread-Safety fixen (kritischstes Stabilitätsproblem)
3. DSLParser-Tests schreiben (kritischste fehlende Test-Abdeckung)

---

## Vorlage für neue Einträge

```markdown
### YYYY-MM-DD – [Kurztitel der Session]

**Agent:** [Agenten-Typ / Name]
**Aufgabe:** [Was war die Aufgabe?]
**Ergebnis:** ✅ Erfolgreich / ⚠️ Teilweise / ❌ Fehlgeschlagen
**Commit:** [Commit-Hash oder PR-Link]

#### Erkenntnisse

- [Was wurde gelernt?]
- [Was war überraschend?]
- [Was hat gut funktioniert?]

#### Fallstricke

- [Was ist schiefgelaufen?]
- [Was muss beim nächsten Mal beachtet werden?]

#### Empfehlungen für nächste Session

1. [Konkrete nächste Schritte]
```
