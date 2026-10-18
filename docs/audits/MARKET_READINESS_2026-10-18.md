# NEUROKORE: Prüfung vor einem kommerziellen Release

Stand: 18.10.2026. Geprüfter Quellstand: `9778e0a1b7c37e0f47cd6ad6227aefd4073d67e5`, nach PR #251. Produktversion: 0.6.8-beta.

## Urteil und Aussagegrenzen

Der geprüfte Stand ist noch nicht für den regulären Verkauf freizugeben. Ausschlaggebend sind ein versionierter privater Produktschlüssel, eine unsichere Preset-Längenverarbeitung und Fehler bei Speichern, Lizenzdurchsetzung und Echtzeitverhalten. Weitere Gestaltung der Unit-Animation ist gegenüber diesen Punkten nachrangig.

Das ist eine Prüfung der wichtigen Produktpfade anhand von Quellcode, ausführbaren Web-/Python-Verträgen und aktuellen GitHub-Logs. Kein gebautes Windows-/macOS-Plugin wurde in einer DAW abgenommen. Ein nativer Absturz oder hörbarer Dropout wird deshalb nicht als hier praktisch reproduziert ausgegeben. Der Bericht beansprucht weder vollständige Fehlerfreiheit der nicht beanstandeten Teile noch eine vollständige Sicherheitsprüfung jeder Codezeile.

P0: vor weiterer öffentlicher Auslieferung klären. P1: vor Verkauf beheben beziehungsweise fehlenden Release-Nachweis erbringen. P2: vor 1.0 beheben oder ausdrücklich als Einschränkung behandeln. „Codebefund“ bezeichnet einen nachvollziehbaren Implementierungsfehler, keinen abgeschlossenen DAW-Reproduktionstest.

## Sicherheits- und Funktionsbefunde

### R01 · P0 · Privater Produktschlüssel ist versioniert

**Beleg:** `tools/license_issuer/LicensePrivateKey.h`, `src/licensing/LicensePublicKey.h`, `tools/license_issuer/Main.cpp`.

Der Issuer enthält einen echten privaten RSA-Schlüssel im getrackten Quelltext. Sein Modulus stimmt mit dem öffentlichen Schlüssel des Plugins überein. Eine lokale mathematische Signatur-/Verifikationsprobe mit einer neutralen Testzahl war erfolgreich. Schlüsselwerte wurden weder ausgegeben noch in diesen Bericht übernommen. Der Schlüssel ist somit nicht lediglich ein unpassendes Beispiel.

**Folge:** Wer diesen Quellstand besitzt, kann grundsätzlich Signaturen erzeugen, denen das Produkt vertraut. Dass das Packaging den Issuer ausschließt, behebt einen im Repository offengelegten Signierschlüssel nicht.

**Maßnahme:** Schlüssel als offengelegt behandeln. Neues Schlüsselpaar außerhalb des Repositories erzeugen, Issuer auf sichere externe Schlüsselzuführung umstellen, öffentlichen Produktschlüssel austauschen und bestehende Tester-Lizenzen kontrolliert neu ausstellen. Akzeptanz alter Schlüssel bewusst entscheiden. Das bloße Löschen der Datei oder Bereinigen der Historie ersetzt die Rotation nicht. Zusätzlich das selbst entwickelte RSA-Signaturformat und die 1024-Bit-Schlüsselerzeugung durch ein geprüftes zeitgemäßes Verfahren ersetzen. Hier wurde kein kryptografischer Angriff auf RSA behauptet oder versucht.

**Abnahme:** Repository und Pakete enthalten keinen privaten Schlüssel. Nur der gesicherte Issuer kann eine vom Release akzeptierte Lizenz erzeugen. Alte kompromittierte Signaturen werden entsprechend der festgelegten Migration abgelehnt.

### R02 · P0 · Preset-Entschlüsselung vertraut einer ungeprüften Größe

**Beleg:** `src/utils/PresetManager.cpp`, `PresetManager::decrypt`, insbesondere `originalSize`, `dest.setSize(originalSize)` und `dest.copyFrom(..., originalSize)`.

Die unverschlüsselte Größenangabe wird nicht gegen `cipherSize` und eine maximale Zustandsgröße geprüft. Ein kleines STAT-Payload kann eine wesentlich größere Zielallokation und einen Kopiervorgang über die vorhandenen Quellbytes hinaus auslösen. Auch ein leerer Cipher-Teil ist durch die Modulo-Prüfung allein nicht ausgeschlossen. JUCE 8.0.6 `MemoryBlock::copyFrom` begrenzt die Zielgröße, kennt aber die Größe des übergebenen Quellbereichs nicht und verwendet `memcpy`.

**Folge:** Beschädigte oder manipulierte importierte Presets können die DAW durch unzulässige Speicherzugriffe oder übermäßige Allokation gefährden. Der gefährliche Codepfad ist belegt, ein nativer Host-Crash wurde hier nicht ausgeführt.

**Maßnahme:** Vor Allokation Mindestlänge, Blockausrichtung, `originalSize <= cipherSize`, maximale Preset-/State-Größe und sichere Integer-Konvertierungen prüfen. Datenformat und Version validieren. Ungültige Dateien ohne Zustandsänderung zurückweisen. Verschlüsselung mit einem festen eingebetteten Schlüssel ist keine Integritätsprüfung.

**Abnahme:** Native Negativtests und ASan/UBSan für verkürzte Dateien, Null-Cipher, übergroße Längen, widersprüchliche Chunk-Angaben und zufällige Eingaben. Vorheriger Soundzustand bleibt erhalten.

### R03 · P1 · Erneutes Speichern überschreibt ein Preset nicht zuverlässig

**Beleg:** `src/utils/PresetManager.cpp`, `savePreset`; `src/bridge/HostSnapshot.cpp`, `applyPresetCmd`.

`savePreset` öffnet `juce::FileOutputStream out(file)` ohne Positionierung oder Trunkierung. JUCE öffnet vorhandene Dateien am Ende. Der Save-Befehl liefert für denselben Namen wieder denselben Dateipfad. Neue Header und Chunks werden somit angehängt, während der Loader weiterhin den alten Header am Dateianfang verwendet. Die bisherigen Tests speichern überwiegend in neue Dateien. Zusätzlich werden Schreib-/Flush-Fehler vor `return true` nicht vollständig ausgewertet.

**Folge:** Der Nutzer kann eine erfolgreiche Speicherung annehmen und beim Wiederladen einen alten Stand erhalten. Direktes Schreiben bietet außerdem keine sichere Ersetzung bei Abbruch oder vollem Datenträger.

**Maßnahme:** Vollständiges Preset in eine temporäre Datei schreiben, alle Statuswerte prüfen und vorhandene Datei atomar ersetzen. Überschreiben und Save As eindeutig behandeln.

**Abnahme:** Dasselbe Preset mehrfach mit geändertem Script, sechs Makros und IR speichern, Prozess schließen und letzten Stand laden. Fehler bei Schreibrechten, vollem Datenträger und unterbrochenem Speichern dürfen den alten Stand nicht zerstören oder Erfolg melden.

### R04 · P1 · IR-Daten aus dem Projektzustand umgehen Importgrenzen

**Beleg:** `src/core/PluginProcessor.cpp`, `setStateInformation` gegenüber `installIr`; `src/core/Config.h`, `kIrMaxSeconds`.

Dateiimport begrenzt auf zwei Kanäle und zwei Sekunden. State-Restore übernimmt dagegen `ch`, `n` und `sr` weitgehend ungeprüft, allokiert danach und akzeptiert auch nicht passende Datenlängen. Die bestehende IR-Bank wird vorher gelöscht und APVTS wird vor abschließender Script-Validierung ersetzt.

**Folge:** Defekte Projekt-/Presetdaten können große Allokationen, unvollständige IRs und teilweise ersetzte Zustände verursachen. Fehlgeschlagenes Restore ist nicht als vollständige Transaktion abgesichert.

**Maßnahme:** Einen gemeinsamen begrenzten IR-Decoder für Datei- und State-Import verwenden. Kanalzahl, endliche plausible Sample-Rate, Länge, Bytezahl und Samples vor Veröffentlichung validieren. Neuen Gesamtzustand erst nach erfolgreicher Prüfung übernehmen.

**Abnahme:** Beschädigte/überdimensionierte IR-States zurückweisen, unveränderten alten Zustand nach Fehler nachweisen und normale eingebettete IRs zwischen Prozessen wiederherstellen.

### R05 · P1 · IR-Vorhören kann einen gleichzeitig gelesenen Buffer verändern

**Beleg:** `src/core/PluginProcessor.cpp`, `startIrPreview` und `mixIrPreview`.

Der UI-Pfad ruft `irPreviewBuf.setSize`, `clear` und `copyFrom` auf. Der Audiothread liest denselben Buffer. `irPreviewPos` ist atomar, der Buffer selbst besitzt dadurch keinen sicheren Ownership-Wechsel. Insbesondere erneut gestartetes Vorhören während laufender Wiedergabe kann mit dem Lesen zusammentreffen.

**Folge:** Datenrennen, Audiostörungen und bei Reallokation potenziell ungültige Speicherzugriffe. Der Race ist ein Codebefund, nicht hier mit ThreadSanitizer reproduziert.

**Maßnahme:** Vorbereiteten unveränderlichen Preview-Snapshot über einen sicheren Ownership-Wechsel übergeben. Alte Daten außerhalb des Audiothreads freigeben. Keine blockierende Audiothread-Sperre ergänzen.

**Abnahme:** Preview wiederholt starten/ersetzen, während Audio läuft und IRs gewechselt werden. ThreadSanitizer beziehungsweise ein gezielter Parallel-Stresstest und Audiothread-Allokationsprobe bleiben sauber.

### R06 · P1 · MIDI-Learn verletzt die eigene Echtzeitregel

**Beleg:** `src/core/PluginProcessor.cpp`, `processBlock`; `src/ui/MidiLearnManager.cpp`, `processMidiMessages` und `setMapping`.

Der Audiocallback ruft MIDI-Learn auf. Beim ersten erlernten CC werden blockierende SpinLocks genommen und Maps mit Strings geändert. `setMapping` verwendet unter anderem `toStdString` und einfügende Map-Zugriffe. Der normale Playback-Pfad besitzt zwar einen TryLock, der Learn-Pfad jedoch nicht.

**Folge:** MIDI-Learn kann gerade während laufenden Audios blockieren oder allokieren. Die vorhandenen DSP-Blocktests beweisen diesen UI-/Processor-Pfad nicht.

**Maßnahme:** Feste vorberechnete CC-Tabelle im Audiothread verwenden. Den Lernauftrag über einen begrenzten Eventkanal an den UI-/Control-Thread melden. Mapping-Verwaltung und Strings dort bearbeiten.

**Abnahme:** Ersten CC bei aktivem Learn unter 32/64-Sample-Blöcken auslösen. Keine Allokation und kein blockierender Lock im vollständigen `processBlock`-Pfad, auch bei parallelem Mapping-Editor.

### R07 · P1 · Abgelaufene Demo wird im normalen DSP-Pfad nicht trocken erzwungen

**Beleg:** `src/core/PluginProcessor.cpp`, `processBlock`; `src/core/DspEngine.cpp`, `processPreparedBlock`; CMake-Definition `NEUROKORE_SKIP_LICENSE_ENFORCEMENT=1` am nativen Testtarget.

Der Processor setzt nach Demoende nur seine lokale Variable `mix` auf null. Die aufgerufene Engine liest `dryWet` erneut aus APVTS und verwendet diesen Wert für ihren Wet-Smoother. Die lokale Variable steuert später nur einen Lock-Fallback-Zweig. Eine entsprechende Lizenzbegrenzung der Engine wurde im untersuchten Pfad nicht gefunden.

**Folge:** Die dokumentierte Regel „Demo beendet, Mix bleibt dry“ ist im normalen Engine-Pfad nicht durchgesetzt. Der zentrale native Testbuild schaltet Enforcement ausdrücklich ab und kann diesen Produktfall nicht bestätigen.

**Maßnahme:** Einen effektiven, geglätteten Mix im DSP-Pfad berechnen, der sowohl Parameter als auch Freigabestatus berücksichtigt. Hostparameter nicht dauerhaft verfälschen. Vollständig PDC-ausgerichteten Dry-Pfad beibehalten.

**Abnahme:** Separater Produktkonfigurationstest ohne Skip-Makro für frische Demo, Ablauf bei laufendem Audio, abgelaufenen Offline-Render, gültige/ungültige Lizenz und Aktivierung. Nicht nur Statusanzeige testen.

### R08 · P1 · Online-Aktivierung meldet Speicherung ohne lokalen Import

**Beleg:** `web/src/overlays/LicensePanel.tsx`, `activate`; `web/src/overlays/licenseModel.ts`; native Importfunktion in `LicenseManager.cpp`.

Die UI sendet Key, Mail und System-ID an die Aktivierungs-URL. Bei jeder erfolgreichen HTTP-Antwort meldet sie „License stored“. Sie liest weder einen Lizenzinhalt aus der Antwort noch führt sie einen nativen verifizierten Import aus. Die vorhandenen nativen Aktivierungspfade importieren dagegen eine lokale Datei. Ein entfernter Webserver kann den lokalen Lizenzspeicher nicht durch eine bloße HTTP-200-Antwort aktualisieren.

**Folge:** Ein Käufer kann eine Erfolgsmeldung erhalten und trotzdem im Demo-Modus bleiben. Backend-Verfügbarkeit, CORS und Kaufprozess wurden nicht praktisch mit Kundendaten getestet.

**Maßnahme:** Definierten API-Vertrag bis zur nativen Signaturprüfung und atomaren Speicherung umsetzen. Erfolg erst nach bestätigtem Produktstatus melden. Alternativ Online-Aktivierung aus dem Produkt entfernen und den funktionierenden Offline-Prozess eindeutig anbieten.

**Abnahme:** Staging-Kauf mit Testschlüssel, Erfolg nach Plugin-Neustart, Fehlerantworten, ungültige Signatur, Timeout und Netzwerkausfall. Keine echten Kundendaten für automatisierte Tests verwenden.

### R09 · P1 · Native Save-/Load-Fehler werden im Frontend verschluckt

**Beleg:** `src/bridge/WebViewHolder.cpp`, `failVar`; `web/src/bridge/juce.ts`; `web/src/presets/presetActions.ts`; `web/src/overlays/PresetExplorer.tsx`, `commitSave`.

Native Fehler werden als erfolgreich aufgelöste Antwort mit `ok:false` zurückgegeben. `presetAction` wertet das Ergebnis nicht aus. `commitSave` wartet zudem nicht auf Abschluss und schließt den Speicherdialog sofort. Zwei temporäre Vitest-Negativtests mit echtem `presetAction` und einer fehlerhaften nativen Antwort bestätigen: Save und Load lösen mit `undefined` auf, statt den Fehler weiterzugeben.

**Folge:** Nutzer bemerken fehlgeschlagene Speicher-/Ladevorgänge nicht zuverlässig. Die Vorschau ohne native Bridge deckt diesen Fehler nicht ab.

**Maßnahme:** Typisiertes Erfolg-/Fehlerprotokoll bis zum aufrufenden Dialog auswerten. Pending-Zustand anzeigen, Fehler lesbar halten, Dialog nur nach bestätigtem Erfolg schließen und Dirty-State nicht vorzeitig löschen.

**Abnahme:** Schreibschutz, fehlende Datei, fehlerhafte Lizenz/IR, Backendfehler und Timeout über den realen Bridge-Vertrag testen.

### R10 · P1 · MIDI-Ereignisse verlieren ihre zeitliche und Noten-Semantik

**Beleg:** `src/core/MidiVariableMapper.cpp`, `processMidi`; `src/core/PluginProcessor.cpp`, Aufruf vor kompletter Blockverarbeitung.

Der Mapper verarbeitet alle Ereignisse vor dem Audioblock, ohne `metadata.samplePosition` zu berücksichtigen. Note On und Note Off im selben Block können somit für die gesamte Verarbeitung nur den letzten Gate-Zustand hinterlassen. Jede Note-Off-Nachricht löscht Gate und Velocity, unabhängig davon, welche andere Note noch gehalten wird. Controller für All Notes Off/All Sound Off sowie ein klarer Reset-Pfad sind nicht vorhanden.

**Folge:** Kurze Noten, überlappende Noten und Stop/Panic können MIDI-gesteuerte Oscillator-/Envelope-Presets unzuverlässig machen. Dieser Befund betrifft die eigenen DSL-MIDI-Variablen, nicht eine pauschale Behauptung über alle Host-Automation.

**Maßnahme:** Ereignispositionen im Processing berücksichtigen, eine definierte monophone Notenpriorität beziehungsweise gehaltene Noten führen und Panic/Reset behandeln.

**Abnahme:** Note On/Off innerhalb desselben Blocks, zwei gehaltene Noten mit frühem Note Off, verschiedene Blockgrößen und Panic bei laufendem Gate vergleichen.

### R11 · P1 · Der Dry-Fallback bei Script-Lock verlässt die Latenzachse

**Beleg:** `src/core/PluginProcessor.cpp`, `processBlock`, Zweig `else if (mix <= 1.0e-5f)`.

Wenn die Script-Sperre nicht verfügbar und Mix null ist, bleibt der rohe Eingang nach einer Finite-Prüfung stehen. Die Engine und ihre Dry-Latenzausrichtung werden dann nicht ausgeführt. Der Host kann weiterhin OS-/IR-/Sanitation-Latenz kompensieren.

**Folge:** Gerade bei Preset-/Formelwechsel im Dry-Zustand kann ein Block auf einer anderen Zeitachse ausgegeben werden. Das ist kein allgemeines Urteil, dass der normale Mix-0-Pfad ungepuffert wäre: dieser besitzt bereits Delay-Alignment.

**Maßnahme:** Einen unabhängig vom austauschbaren Graphen vorbereiteten, konsistent verzögerten Dry-/Continuity-Pfad verwenden.

**Abnahme:** Bei gemeldeter Latenz absichtlich Script-Lock belegen und einen zeitlich definierten Impuls bei Mix 0 einspeisen. Peakposition und Verlauf müssen zur ungestörten Ausgabe passen.

### R12 · P1 · VST3-Manifest beschreibt nicht alle Factory-Klassen

**Beleg:** `scripts/write_vst3_moduleinfo.mjs`, `moduleInfo`; JUCE 8.0.6 `juce_audio_plugin_client_VST3.cpp`, `getClassEntries`.

Das eigene Manifest schreibt Component und Controller. Die gepinnte JUCE-Version registriert außerdem eine Compatibility-Klasse. Der CID-Generator kennt deren Typ, verwendet ihn aber nicht im JSON. Die lokalen Tests prüfen im Wesentlichen Name und enthaltene Component-CID, nicht die vollständige Übereinstimmung von Manifest und tatsächlicher Factory.

**Folge:** Die für manche Hosts wichtige Scanner-Metadatenbasis ist unvollständig. Daraus wird hier kein bereits auf jeder DAW reproduzierter Scanfehler abgeleitet.

**Maßnahme:** Manifest gegen die echte Factory des finalen Binaries prüfen und alle angebotenen Klassen korrekt aufführen. Historische ausgelieferte Component-/Controller-CIDs als feste Upgrade-Referenz erfassen. Der CMake-Schalter `JUCE_VST3_CAN_REPLACE_VST2=0` widerspricht dem Freeze-Hinweis in AGENTS.md. Nicht blind den Schalter zurückändern: bestehende Binäridentität und Projektkompatibilität sind die Referenz.

**Abnahme:** Factory-Enumeration, Manifestvergleich, Steinberg-Validator und echtes Upgrade einer gespeicherten Session aus der älteren ausgelieferten Version. Kein Verlust von Instanz oder Automation.

### R13 · P1 · Packaging trägt widersprüchliche Versionsdaten

**Beleg:** `scripts/package_alpha.py` nennt Archiv und BUILD.json `0.6.4-beta`; `docs/ALPHA_INSTALL.txt` nennt `0.6.6-beta`; `scripts/build_local.sh` meldet `0.6.5-beta`; CMake/Windows-Installer stehen auf `0.6.8-beta`.

**Folge:** Das manuelle native CI kann einen aktuellen Build unter veraltetem Namen mit falschem Build-Manifest ausliefern. Support und Tester können Paketstand und Produktstand verwechseln.

**Maßnahme:** Version und Build-Commit aus einer einzigen Quelle an alle Pakete, Handbücher und Manifeste durchreichen. Nicht die Produktversion als Teil dieses Audits erhöhen.

**Abnahme:** Erstelltes Archiv entpacken und Binary-Version, BUILD.json, Installer, Anzeigename und Handbuch automatisiert vergleichen.

### R14 · P2 · Preset-Reload behandelt nur vier von sechs Makronamen

**Beleg:** `src/utils/PresetManager.cpp`, `namesBefore`, `namesFromState` und Schleifen `< 4` nach DSCR-Reapply. `Config::kNumUserParams` ist sechs.

**Folge:** Benutzerdefinierte Namen von E/F werden nicht mit derselben Wiederherstellung wie A–D behandelt. Vorherige Namen können außerdem im Fallback in ein anderes Preset übernommen werden.

**Maßnahme:** Einen vollständigen Preset-Zustand für alle sechs Makros wiederherstellen. Keine Namen aus dem zuvor aktiven Preset als stillen Ersatz verwenden.

**Abnahme:** Sechs individuelle Labels und Parameterwerte speichern, anderen Sound laden, Prozess neu starten und exakt zurückladen.

### R15 · P2 · Kleine manuell gesetzte Fenstergröße wird beim Öffnen verworfen

**Beleg:** `src/ui/WebPluginEditor.cpp`, Konstruktor; `src/core/Config.h`, `kUiMinWindowWidth=1100`, `kUiMinWindowHeight=739`.

Die native manuelle Größenbegrenzung erlaubt Breite 800. Der Wiederherstellungspfad akzeptiert gespeicherte Größen erst ab 1100×739. Ein zulässig verkleinertes Fenster wird beim erneuten Öffnen deshalb nicht in dieser Größe wiederhergestellt.

**Maßnahme:** Dieselben konsistenten Grenzen für Drag-Resize, Speicherung und Wiederherstellung verwenden.

**Abnahme:** Im echten Host auf 800/960/1100 Pixel Breite verkleinern, Editor schließen und wieder öffnen. Zusätzlich DPI-Wechsel, maximiertes Fenster und neue Projektinstanz testen. Die bisherigen Browser-Resize-Verträge allein decken das native Wiederöffnen nicht ab.

### R16 · P1 · Installer und Runtime-Ausfall sind nicht robust abgeschlossen

**Beleg:** `installer/NeuroKore.iss`, `WebView2Installed`, `PrepareToInstall`, `RemoveOldVersionedBundles`; `src/bridge/WebViewHolder.cpp`, `constructSurface`.

Die Runtime-Erkennung prüft lediglich, ob Registry-Schlüssel existieren. Microsoft verlangt eine gültige `pv`-Version beziehungsweise eine erfolgreiche Runtime-Abfrage. Ein leerer Restschlüssel reicht im aktuellen Installer zur Annahme „installiert“. Wenn die WebView-Optionen nicht unterstützt werden, erzeugt das Plugin nur eine leere Component. Außerdem werden alte versionierte VST3-Verzeichnisse beim Installationsschritt unabhängig davon entfernt, ob überhaupt VST3 als Komponente gewählt ist. Der Bootstrapper setzt für eine bisher fehlende Runtime Internetzugang voraus.

**Folge:** Leere Plugin-Fenster ohne brauchbare Diagnose, problematische Offline-Installation und unerwartete Entfernung alter VST3-Installationen auch bei Standalone-only-Auswahl sind möglich.

**Maßnahme:** Runtime-Version zuverlässig prüfen und nach Installation erneut verifizieren, Offline-Weg anbieten, leere Oberfläche durch eine native Diagnose mit Reparaturhinweis ersetzen. Migration alter VST3-Bundles an die gewählte Komponente und eine getestete Upgrade-Regel binden.

**Abnahme:** Saubere Windows-VM, kaputte/fehlende Runtime, Offline-System, normaler Benutzer, Standalone-only, VST3-only, Update und Uninstall. Anwenderdaten und nicht ausgewählte Komponenten bleiben erhalten.

### R17 · P1 · Release-Gates sind unvollständig und aktuell nicht grün

**Beleg:** `.github/workflows/native-alpha.yml`, `.github/workflows/web-layout.yml`, aktueller Webtestlauf und GitHub-Job 117526625793.

Native Builds, native Tests und pluginval laufen nur bei `workflow_dispatch`. Der PR-Workflow führt den Web-Build und Playwright aus, aber nicht die vollständige Vitest-Suite. Auf dem geprüften Main bestehen 717 Webtests, vier schlagen fehl. `test_factory_curation.py` scheitert zusätzlich am Preset „Sub Zero Pummel“. Der Layoutlauf zu PR #250 endete mit 17 bestandenen und 11 fehlgeschlagenen Tests nach 16,8 Minuten. Logs enthalten Boot-/RAF-Timeouts. Der neuere Lauf zu #251 war beim Abruf noch in Arbeit. Der Fehlergrund ist damit nicht pauschal als reiner Layoutdefekt oder bloßes Infrastrukturproblem geklärt.

**Maßnahme:** Zuverlässige, begrenzte PR-Gates für Web-, native Kern- und Source-Verträge einrichten. Layout und gezielte GPU-Tests mit definiertem Software-Renderer/Parallelitätsbudget getrennt reproduzierbar betreiben. Vor Release alle unterstützten Plattformen aus einem festgelegten Commit bauen und validieren. Den tatsächlich erforderlichen Check im Repository-Ruleset erzwingen; dessen aktuelle Konfiguration wurde hier nicht abgefragt. Fehler sauber korrigieren, nicht Tests löschen oder Timeouts beliebig erhöhen.

**Abnahme:** Grüne reproduzierbare Gates und aufbewahrte Logs/Artefakte für genau den Release-Commit. Ein Browser-Preview-Pass ersetzt keinen nativen Plugin-Pass.

### R18 · P1 · Verkaufspakete besitzen keinen durchgehend erzwungenen Signierprozess

**Beleg:** `scripts/package_alpha.py`, `scripts/sign_windows.ps1`, `scripts/package_windows.ps1`, `.github/workflows/native-alpha.yml`.

Mac-Pakete werden nur ad hoc signiert. Developer-ID-Signierung und Notarisierung sind nicht implementiert. Windows-Signieren ist optional und überspringt fehlendes signtool sogar dann, wenn die Release-Umgebung eigentlich signieren sollte. Der Installer wird nicht im nativen CI als fertig installierbares Verkaufspaket gebaut und getestet.

**Maßnahme:** Alpha- und kommerziellen Release-Modus trennen. Im Verkaufskanal bei fehlender Signatur abbrechen, Signaturen nach dem Verpacken prüfen und macOS-Pakete über Developer ID, Hardened Runtime, Notarisierung und passende Ticket-Auslieferung führen. Verfügbare Zertifikate/Konten wurden nicht geprüft.

**Abnahme:** Download der fertigen Pakete auf sauberen Windows-/macOS-Systemen, Installation unter den vorgesehenen Sicherheitseinstellungen und erfolgreiche DAW-Erkennung. Signatur-/Notarisierungsberichte zum Commit archivieren.

### R19 · P1 · Drittanbieter- und Asset-Nachweise sind nicht paketweit abgesichert

**Beleg:** `web/public/fonts/apex.otf`, `web/public/fonts/JetBrainsMono-Regular.ttf`, `Resources/irs/*.wav`, `scripts/package_windows.ps1`, `scripts/package_alpha.py`, `LICENSE`.

Der Windows-Staging-Pfad kopiert die eigene LICENSE und EULA, aber keine vollständig erzeugte Drittanbieter-Lizenzsammlung. Das Alpha-Script sammelt manche vorhandenen Dateien, prüft damit jedoch keine vollständige Lizenzinventur aller npm-Abhängigkeiten und Assets. Für Apex-Schrift und IR-Assets wurden im Repository keine zugehörigen Freigaben aufgefunden. Das ist kein Beweis, dass diese Rechte nicht extern vorliegen. Ebenso kann der persönliche JUCE-Lizenzstatus nicht aus dem Repository festgestellt werden.

**Maßnahme:** Nachweise für JUCE, VST3-SDK, npm-Bibliotheken, Schriften, Bilder und IRs erfassen. Erforderliche Hinweise im tatsächlichen Installer/Archiv mitliefern. Vertriebslizenz und Eigentum an Assets dokumentieren. Bestehende Rechte prüfen, statt ohne Grund alle Assets zu ersetzen.

**Abnahme:** Bestandteile der ausgelieferten Binär-/Webassets mit einer Lizenzinventur abgleichen. Paket enthält die erforderlichen Notices. JUCE-8-Vertriebsgrundlage ist dokumentiert.

## Bewusste Produktentscheidung mit Einfluss auf Projekt-Recall

Oversampling, Soft Clip und Live/Studio sind laut Handbuch und Implementierung global. `applySharedProcessingPrefs` überschreibt nach einem Projekt-Restore lokale Werte mit aktuellen Maschinenpräferenzen. Das entspricht der bisherigen Vorgabe und wird nicht als versehentliche Regression eingestuft.

Vor 1.0 muss dennoch verbindlich entschieden werden, was eine gespeicherte Session garantiert: Soll sie auf einem anderen Rechner beziehungsweise nach einer Änderung der globalen Einstellungen gleich klingen? Falls ja, braucht es einen Projekt-/Render-Snapshot oder einen klaren Override-Modus. Ein unabhängiger Offline-Render darf nicht durch eine gerade in einer anderen Instanz veränderte globale Klangpräferenz unbemerkt anders werden. UI-Präferenzen und klangändernde Betriebsarten benötigen dafür getrennte Produktregeln.

## Bereits vorhandene Grundlagen

Es gibt umfangreiche Parser-, Graph-, DSP-Block-, Preset-, Spektral-, Latency-, Sanitation- und Echtzeit-Allokationsverträge. Große Hostblöcke werden im DspEngine-Pfad auf vorbereitete Teilblöcke aufgeteilt. Webassets sind eingebettet, externe Navigation wird abgefangen, der Web-Bridge-Dispatcher ist begrenzt, Preset-Chunks haben bereits einige Dateigrenzprüfungen und die Unit besitzt echte Geometrie- sowie Produktionsshader-Tests. Diese Grundlagen sind nützlich, lösen die oben benannten Lücken aber nicht automatisch.

`npm audit --omit=dev` meldete beim Abruf keine bekannten Advisories in den Produktionsabhängigkeiten. Das bewertet weder den eigenen C++-/Bridge-Code noch Assetrechte oder allgemeine Sicherheit.

## Externe Referenzen

- JUCE 8.0.6 MemoryBlock-Implementierung: https://github.com/juce-framework/JUCE/blob/8.0.6/modules/juce_core/memory/juce_MemoryBlock.cpp
- JUCE FileOutputStream, Append-Verhalten: https://docs.juce.com/master/classjuce_1_1FileOutputStream.html
- JUCE 8.0.6 VST3-Factory und Compatibility-Klasse: https://github.com/juce-framework/JUCE/blob/8.0.6/modules/juce_audio_plugin_client/juce_audio_plugin_client_VST3.cpp
- Steinberg Validator/Test Host: https://steinbergmedia.github.io/vst3_dev_portal/pages/What%2Bis%2Bthe%2BVST%2B3%2BSDK/Plug-in%2BTest%2BHost.html
- Microsoft WebView2-Distribution und Runtime-Erkennung: https://learn.microsoft.com/microsoft-edge/webview2/concepts/distribution
- Apple Developer ID/Notarisierung: https://developer.apple.com/developer-id/
- JUCE-8-Lizenzbedingungen: https://juce.com/legal/juce-8-licence/
