# Auszüge: `boulder-v1/screens-0.52/index.md`

KEINE vollständige Datei. Die Datei selbst wurde von Workflow-Agenten geschrieben, deren Schreibvorgänge nicht im Transkript stehen. Hier stehen nur Befehle der alten Sitzung (Sitzung `session_011udJ51Jg5Y9cBcFvGfK26z`), die Teile davon gelesen haben, mit ihrer Ausgabe, wörtlich. Ausgaben können auch andere Dateien betreffen.

## 2026-10-08 16:59:48 (toolu_01NH38uXpNE8Yo4zyAVPxDRV)

Befehl:

~~~~
cd /home/dysseus/Documents/Claude/boulder-v1; grep -rn -i "40[ ',.]\?378\|pace 0\|Pace.*0\b\|Aufstieg\|ascent" design-pruefung-0.52.md screens-0.52/index.md | head
~~~~

Ausgabe:

~~~~
screens-0.52/index.md:194:- **Routen mit Punktabstand über 5 s** (Glättungsfenster ±5 s): Das Tempo fällt auf 0, „Moving“ auf 0. Beim Wandern stand dann „40,378 m/h“ Aufstieg.
~~~~

## 2026-10-08 16:59:51 (toolu_01B377xbRyLymnxJ3kf9wCMz)

Befehl:

~~~~
cd /home/dysseus/Documents/Claude/boulder-v1; sed -n 185,200p screens-0.52/index.md
~~~~

Ausgabe:

~~~~
  - Titel: „Run“, „Ride“, „Hike“, „Walk“, „Swim“.
  - Chips und „By sport“: „Running“, „Bike“, „Hiking“, „Swimming“.
  - Dialog: „Bike Road/Gravel“, „Bike MTB“.
- **Sport-Trends**:
  - Wochennummern („34 … 41“, „Wk 38“) statt Daten wie in den Health-Trends.
  - Der Titel heisst nur „Trends“.
- **Kraft**: „−0 bpm/min · slow recovery“ (negative Null).
- **Yoga**: Die Zonenkarte enthält nur „below zone 1 · 100%“.
- **Dialog „What was it?“**: Ein Material-AlertDialog mit Textknöpfen statt Zeilen aus dem Baukasten.
- **Routen mit Punktabstand über 5 s** (Glättungsfenster ±5 s): Das Tempo fällt auf 0, „Moving“ auf 0. Beim Wandern stand dann „40,378 m/h“ Aufstieg.
  - Gesehen im ersten Versuch mit 10-s-Punkten.
  - Die Bilder hier haben 5-s-Punkte. Betroffen wären Routen fremder Apps mit dünnem GPS.

### Übrige
- **Stack, Koffeinkurve**:
  - Kein Raster, keine Achse rechts.
~~~~
