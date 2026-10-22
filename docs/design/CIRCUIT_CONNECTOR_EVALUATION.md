# Circuit: breite Chips und obere Anschlüsse

Stand 2026-10-22, main nach PR #326. Bewertung, keine Änderung der Anschlussgeometrie.

## Befund

Die Breite kommt aus den vollständigen Beschriftungen auf einer einzigen südlichen Parameterleiste. Das ist unabhängig von der kleinen LED-Matrix in der Funktionsfläche. Gemessen mit `hydrateBoard` und den aktuellen ChipSpec-Standardargumenten, in logischen Board-Pixeln:

| Chip | Breite | Höhe | Südliche Parameteranschlüsse |
| --- | ---: | ---: | ---: |
| StepSeq | 1536 | 256 | 15 |
| Comp | 1024 | 256 | 10 |
| Filter | 352 | 256 | 2 |
| Meter | 352 | 256 | 0 |

Die Breite berechnet `captionedChipWidth` aus Textlänge, Raster und tatsächlichen Anschlussankern. `chipBox` reserviert außerdem Platz für Typcode und Bedienelemente. StepSeq führt auch die vier Legacy-Schrittparameter weiter als Anschlüsse. Die Anzahl frei editierbarer Parameter muss nicht der Anzahl ständig sichtbarer Modulationseingänge entsprechen.

## Möglichkeiten

| Variante | Nutzen | Aufwand und Auswirkungen |
| --- | --- | --- |
| Wenige primäre Eingänge, weitere per Inspect/aufklappbarer Anschlussleiste | Größter Breitengewinn bei unverändertem Signalfluss | Sichtbarkeit als eigener Zustand, stabile Port-IDs, verbundene Anschlüsse immer sichtbar halten. Gespeicherte Modulationsverbindungen dürfen weder verschwinden noch umadressiert werden. |
| Parameter auf obere und untere Leiste verteilen | Mehr Anschlüsse bei ungefähr halber Beschriftungsbreite möglich | Etwas höhere Chips. Eigene obere Leiste mit Reservierung für Titel und Bedienelemente. Neues Portmodell und Anpassung aller Routing- und Interaktionspfade. |
| Mehrzeilige Parameterfelder auf der Fläche | Kompakte Kontur und mehr Text | Verdrängt Funktionsdarstellung. Leitungen dürfen nicht quer durch Regler/Funktionsfläche geführt werden. Für dauerhaft sichtbare Ports schlechter geeignet. |
| Kurze, dokumentierte Parameternamen | Geringer Eingriff | Spart begrenzt Platz. Vollständige Namen im Tooltip und Inspect nötig. Keine willkürlichen Abkürzungen für seltene Parameter. |

## Empfehlung

Zuerst die für Modulation sinnvollen Hauptanschlüsse je Modul festlegen. Beim StepSeq beispielsweise Rate, Gate, Glide und Probability priorisieren, während Listen, Richtung, Seed und Reset in Inspect gepflegt werden. Die konkreten Anschlussentscheidungen müssen gegen die unterstützten Modulationsziele geprüft werden. Bestehende Verbindungen und Legacy-Aliase bleiben adressierbar.

Danach obere und untere Parameterleiste für Module mit weiterhin vielen sinnvollen Eingängen. Audio bleibt links/rechts, Modulation erhält eine eigene obere/untere Fläche. Das erhält die Leserichtung. Eine frei wählbare Seite für jeden Audioanschluss wäre für dieses Problem unnötig aufwendig.

## Technische Voraussetzungen für obere Ports

- `BoardPort` beschreibt bislang Ost/Süd über boolesche Eigenschaften. Ein explizites `face: west | east | north | south` sollte Geometrie, Kontaktmalerei und Anschlussnormalen gemeinsam führen.
- `portLocal`, `portGlobal`, `countFaces`, `graphToLayout` und `chipZones` müssen dieselbe Seitenbelegung nutzen.
- `cableFace` kennt den Typnamen top, liefert aktuell für Parameter aber bottom. Mehrere Stellen zählen top und bottom gemeinsam als Südanschlüsse. Nur die CSS-Position zu verschieben wäre falsch.
- Die freie Verbindungsvorschau verwendet derzeit Ost/West-Normalen. Magnetische Trefferflächen und Vorschau brauchen alle vier Richtungen.
- Der Router muss Nord/Süd-Start- und Endstubs sowie die belegten Beschriftungszonen berücksichtigen. Titel und Regler dürfen keine Kabelkorridore werden.
- Chip-Abmessungen, Textzonen, Reservekanäle und Kamera-Fit müssen aus demselben Geometriemodell folgen.
- Bestehende Patches behalten Port-IDs und manuelle Positionen. Änderungen erfolgen als explizite Layoutaktion, nicht während einer Reglerbewegung.

Abnahmekriterien: eindeutige lokale Portzentren, kollisionsfreie Beschriftungen bei 1×/2× und allen Themes, vierseitige Drag-/Magnetprüfung, keine Chip-Durchquerung, stabile manuelle Positionen, unveränderte DSL-Verbindungen und dokumentierte Migration der sichtbaren Leisten.
