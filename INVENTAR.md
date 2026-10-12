# Inventar der Rettung

Quelle: Transkript der archivierten Sitzung `session_011udJ51Jg5Y9cBcFvGfK26z` (26.09. bis 08.10.2026).
Gerettet am 12.10.2026. Zeiten in MESZ (Transkript in UTC, +2 h).

Vorgehen: Die Schreibvorgänge der Sitzung (Write, `cat >`/`cat >>`-Heredocs, `printf >>`, Python-Ersetzungen)
wurden in zeitlicher Reihenfolge nachgespielt, die Pfade nach `rettung/` umgelegt. Volle Read-Ausgaben dienten als
Ausgangsstand. Alle nachgespielten Ersetzungen griffen (keine `assert` schlug fehl). Gegenprobe: Zeilen 32–40 von
`entscheidungen.md` stimmen mit einer `sed`-Ausgabe der alten Sitzung überein; die sechs `.dc.html` und `canvas.json`
haben genau die Bytegrössen aus dem `ls`/`wc -c` der alten Sitzung.

Status:
- **vollständig**: letzter bekannter Stand ganz rekonstruiert.
- **teilweise**: nur Auszüge vorhanden (unter `auszuege/`), keine ganze Datei.
- **nur erwähnt**: Name bekannt, Inhalt nicht im Transkript.

## boulder-v2

| Datei | Status | Letzte Änderung | Bemerkung |
|---|---|---|---|
| `entscheidungen.md` | vollständig | 08.10. 13:07 | Basis 05.10. 17:04, danach 15 Änderungen (Anhänge und Ersetzungen), zuletzt „F8–F10“. Workflows haben sie nur gelesen. |
| `ideen.md` | vollständig | 08.10. 12:09 | Basis 05.10., Anhänge bis „Hypnogramm im Schlaf-Pin“, PR-#2314-Vermerk eingesetzt. |
| `naechte/notizen.md` | vollständig | 08.10. 06:43 | Zwei Anhänge (neu angelegt 06:37). |
| `naechte/nacht-2026-10-05.csv` | vollständig | 06.10. 06:28 | Write aus dem Transkript. |
| `naechte/nacht-2026-10-05-morgen.csv` | vollständig | 06.10. 07:28 | Von einem Python-Skript aus `nacht-2026-10-05.csv` erzeugt; Skript nachgespielt (718 Minutenzeilen wie im Original). |
| `naechte/nacht-2026-10-07.csv` | vollständig | 08.10. 06:27 | Write aus dem Transkript. |
| `v2-schritt3-bau.wf.js` | vollständig | 08.10. 19:26 | Volle Read-Ausgabe des Workflow-Skripts + Python-Umbau (WIP-Hinweis, Gegenprüfung in sechs Blickwinkeln), dann hierher kopiert. 79 Zeilen wie im Original. |
| `schritt3-spezifikation.md` | teilweise | 08.10. 11:00 (Workflow `v2-schritt3-ueberarbeiten`, 76 193 B) | Von Workflow-Agenten geschrieben. Nur Auszüge: Zusammenfassung des Workflow-Ergebnisses, Überschriftenliste, Zeilen 80–120 und 466–530. Ganze Datei nicht rettbar. |
| `schritt2-spezifikation.md` | teilweise | 07.10. 12:02 | Von einem Workflow geschrieben. Gerettet: angehängter Abschnitt „10. Entscheide des Nutzers“ (`auszuege/boulder-v2/schritt2-spezifikation.abschnitt-10.md`), Zeilen 258–296, F-/AP-/Überschriftenlisten. |
| `schritt1-abnahme.md` | teilweise | 06.10. 23:19 (6 384 B) | Überschriften und Kriterium 4 aus `grep`, Fitbit-Treffer. |
| `schritt1-spezifikation.md` | teilweise | unbekannt (Workflow 05.10.) | Nur `grep`-Treffer zu Fitbit. |
| `schritt1-grundlage.md` | teilweise | unbekannt | Eine Zeile (Host-Tests). |
| `fakten-innen.md` | teilweise | 05.10. 15:25 | `grep`-Treffer und Zeilen 11–32, 55–110. |
| `schlaf-schritte-gegenueberstellung.md` | teilweise | 05.10. 15:25 | Ende ab „## Vorschlag 2“ (aus `sed`). Erzeugt aus Workflow-Ausgabe `wslnpi94o`, die nicht im Transkript steht. |
| `b26-auswertung.md` | teilweise | 06.10. | Erste 3 500 Zeichen. |
| `b26-einbau.md` | teilweise | 06.10. | Anfang und `grep`-Treffer. |
| `schritt2-abnahme.md` | nur erwähnt | 08.10. vor 06:24 | Vom Workflow geschrieben. |
| `schritt3-abnahme.md` | nur erwähnt | im Transkript nie geschrieben | Ziel von AP9 im Bau-Workflow. Der erste Lauf brach am Nutzungslimit ab; der Neustart (08.10. 19:26) endet ausserhalb des Transkripts. |
| `schritt2-bestand-kieselsport.md`, `schritt2-bestand-workout.md` | nur erwähnt | 07.10. | Workflow `v2-schritt2-spezifikation`. |
| `schritt3-bestand-daten.md`, `schritt3-bestand-pebble-health.md` | nur erwähnt | 08.10. | Workflow `v2-schritt3-spezifikation`. |
| `schritt1-inventar.md` | nur erwähnt | unbekannt | |
| `entwurf-garmin.md`, `entwurf-isoliert.md`, `entwurf-plugin.md`, `entwurf-strom.md` | nur erwähnt | 05.10. 15:25 | Aus Workflow-Ausgabe `wslnpi94o` (30 626 / 28 694 / 27 477 / 25 090 B). |
| `fakten-aussen.md`, `gutachten-qualitaet.md`, `gutachten-stabilitaet.md` | nur erwähnt | 05.10. 15:25 | Dito (37 384 / 12 166 / 15 384 B). |
| `b26-aufstehen.md`, `b26-gegenpruefung-1.md`, `b26-gegenpruefung-2.md`, `b26-nachbesserung-1.md` | nur erwähnt | 06.10. | b26-Workflows. |
| `schlafvergleich/vergleich.md`, `schlafvergleich/regel.md`, `schlafvergleich/fitbit-*` | nur erwähnt | 08.10. ~07:35 | Workflow `schlaf-fitbit-vergleich`. |
| `naechte/fitbit-2026-09-30.png`, `fitbit-2026-10-02.png`, `fitbit-2026-10-05.png`, `fitbit-2026-10-07.png`, `boulder-2026-09-30.png` | nur erwähnt | 06.–08.10. | Bilder, aus Uploads kopiert. |
| `belege-schritt2/`, `belege-schritt3/` | nur erwähnt | 08.10. | Belegordner (PNG, Mess-Skripte, Protokolle). |

## boulder-v1

| Datei | Status | Letzte Änderung | Bemerkung |
|---|---|---|---|
| `tagesband-0.51-spezifikation.md` | vollständig | 06.10. 17:34 | Write. |
| `0.52-inputs.md` | vollständig | 06.10. 21:28 | Write + 4 Anhänge + 1 Ersetzung. |
| `korrekturen-0.52-spezifikation.md` | vollständig | 06.10. 21:56 | Write. |
| `0.53-inputs.md` | vollständig | 08.10. 20:05 | Zweimal neu geschrieben, dann 5 Anhänge. Den letzten (Punkt 4, Correlation) gab es nur auf einer nicht gespeicherten Transkriptseite; er wurde von Hand übernommen. |
| `garmin-uebergabe-spezifikation.md` | vollständig | 07.10. 12:48 | Write. |
| `entwurf-correlation/Correlation-Health.dc.html`, `-Leer`, `-Magnesium`, `-Liste`, `-Neu`, `-Koffein-Puls` | vollständig | 08.10. 18:44 | Aus vollen Read-Ausgaben; Bytegrössen stimmen. |
| `entwurf-correlation/canvas.json` | vollständig | 08.10. 18:46 | Aus `cat`-Ausgabe; 1 378 B wie im Original. |
| `correlation-konzept.md` | teilweise | 08.10. (330 Zeilen) | Zeilen 1–35, 155–185, Überschriften und E1–E7, Workflow-Zusammenfassung. |
| `correlation-aufwand.md` | teilweise | 08.10. (258 Zeilen) | Zeilen 1–60 und 124–258, Überschriften. |
| `design-pruefung-0.52.md` | teilweise | 08.10. (351 Zeilen) | Zeilen 1–45, `grep`-Treffer, Workflow-Zusammenfassung. |
| `screens-0.52/index.md` | teilweise | 08.10. | Zeilen 185–200. |
| `karte-0.51.md` | nur erwähnt | 06.10. | |
| `entwurf-erlebnis/Erlebnis-*.dc.html` (10 Dateien) | nur erwähnt | 06.10. 17:33 | Aus dem Scratchpad der alten Sitzung kopiert, Inhalt nicht im Transkript. |
| `design-pruefung-0.52/*.png`, `screens-0.52/*.png` | nur erwähnt | 08.10. | Bilder. |
| `belege-0.51/`, `belege-0.52/`, `belege-garmin/` | nur erwähnt | 06.–07.10. | Bilderordner. |
| `garmin-import/Garmin-Import-1.0.0.apk` | nur erwähnt | 07.10. 14:52 | Binär (sha256 03eff73f…). |

## Workflow-Skripte (`workflows/`)

Alle inline übergebenen Skripte ab 05.10. ~08:00, je Name die letzte Fassung (25 Skripte):
`welle2-gegenlesen`, `review2-fixes`, `review2-abschluss`, `e2e-uhr-app`, `v2-schlaf-schritte-entwuerfe`,
`v2-schritt1-spezifikation`, `b26-einschlafen-regeln`, `b26-aufsteh-regel`, `v2-ap0-geruest`, `v2-ap2-schlafkern`,
`v2-schritt1-rest`, `boulder-051-erlebnis`, `hrv-luecken-untersuchen`, `boulder-052-korrekturen`,
`v2-schritt2-spezifikation`, `v2-wecker-neustart`, `v2-wecker-und-schritt2`, `garmin-uebergabe-app`,
`garmin-import-verlaufsrecht`, `v2-schritt2-nachtrag`, `schlaf-fitbit-vergleich`, `v2-schritt3-spezifikation`,
`v2-schritt3-ueberarbeiten`, `v2-schritt3-bau`, `boulder-screens-und-korrelation` – alle **vollständig**.

- `v2-schritt3-ueberarbeiten.js` enthält beide späteren Python-Ersetzungen (Korrektur, dann Präzisierung zum Koffein);
  `v2-schritt3-ueberarbeiten.erster-start.js` ist die Fassung vom ersten Start.
- `v2-schritt3-bau.js` ist die erste Fassung (11:07); die umgebaute letzte Fassung ist `boulder-v2/v2-schritt3-bau.wf.js`.
- **Nicht erfasst:** Workflow-Skripte vom 26.09. bis 05.10. ~08:00. Dieser ältere Teil des Transkripts wurde nicht
  durchgesehen (rund 100 Ereignisse decken nur etwa 6 Minuten ab). Die Notizordner gab es damals noch nicht
  (`boulder-v2` angelegt am 05.10. 15:25, `boulder-v1` am 06.10. 17:33).

## Auszüge (`auszuege/`)

Für jede Datei mit Status „teilweise“ gibt es eine Datei `auszuege/<pfad>.auszuege.md` mit den Befehlen der alten
Sitzung und ihrer wörtlichen Ausgabe. Diese Dateien sind keine Rekonstruktion der Originale.
