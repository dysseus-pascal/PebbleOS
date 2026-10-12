# Auszüge: `boulder-v2/schlaf-schritte-gegenueberstellung.md`

KEINE vollständige Datei. Die Datei selbst wurde von Workflow-Agenten geschrieben, deren Schreibvorgänge nicht im Transkript stehen. Hier stehen nur Befehle der alten Sitzung (Sitzung `session_011udJ51Jg5Y9cBcFvGfK26z`), die Teile davon gelesen haben, mit ihrer Ausgabe, wörtlich. Ausgaben können auch andere Dateien betreffen.

## 2026-10-05 13:25:36 (toolu_016WmaV4Qmf37EFJWaywy6Gw)

Befehl:

~~~~
cd /tmp/claude-1000/-home-dysseus-Documents-Claude/a0d87cde-04f6-43b4-bc17-a2150aed9501 && mkdir -p ~/Documents/Claude/boulder-v2 && python3 - <<'EOF'
import json,os
d=json.load(open('tasks/wslnpi94o.output'))['result']
base=os.path.expanduser('~/Documents/Claude/boulder-v2/')
open(base+'schlaf-schritte-gegenueberstellung.md','w').write(d['synthese'])
for e in d['entwuerfe']:
    open(base+'entwurf-'+e['key']+'.md','w').write('# '+e['name']+'\n\n'+e['kurz']+'\n\n'+e['entwurf'])
open(base+'gutachten-stabilitaet.md','w').write(d['gutachten']['stabil'] or '')
open(base+'gutachten-qualitaet.md','w').write(d['gutachten']['qualitaet'] or '')
open(base+'fakten-innen.md','w').write(d['fakten']['intern'] or '')
open(base+'fakten-aussen.md','w').write(d['fakten']['extern'] or '')
print(len(d['synthese']))
EOF
ls -la ~/Documents/Claude/boulder-v2/; sed -n '/## Vorschlag 2/,$p' ~/Documents/Claude/boulder-v2/schlaf-schritte-gegenueberstellung.md
~~~~

Ausgabe:

~~~~
15629
total 244
drwxr-xr-x 2 dysseus dysseus  4096 Oct  5 15:25 .
drwxr-xr-x 3 dysseus dysseus    74 Oct  5 15:25 ..
-rw-r--r-- 1 dysseus dysseus 30626 Oct  5 15:25 entwurf-garmin.md
-rw-r--r-- 1 dysseus dysseus 28694 Oct  5 15:25 entwurf-isoliert.md
-rw-r--r-- 1 dysseus dysseus 27477 Oct  5 15:25 entwurf-plugin.md
-rw-r--r-- 1 dysseus dysseus 25090 Oct  5 15:25 entwurf-strom.md
-rw-r--r-- 1 dysseus dysseus 37384 Oct  5 15:25 fakten-aussen.md
-rw-r--r-- 1 dysseus dysseus 34759 Oct  5 15:25 fakten-innen.md
-rw-r--r-- 1 dysseus dysseus 12166 Oct  5 15:25 gutachten-qualitaet.md
-rw-r--r-- 1 dysseus dysseus 15384 Oct  5 15:25 gutachten-stabilitaet.md
-rw-r--r-- 1 dysseus dysseus 15904 Oct  5 15:25 schlaf-schritte-gegenueberstellung.md
## Vorschlag 2: Garmin-Weg – Merkmalsband und Morgenurteil

**Kernidee:** Jede Minute legt die Uhr einen festen Satz von 12–16 Byte in ein Tagesband im Flash. Er enthält Bewegung, einen feinen Handwinkel, Puls, RMSSD aus den RR-Werten, SpO2, Licht und Kontext. Ein kleiner Live-Teil entscheidet nur, was sofort gebraucht wird: Schlafzustand, Wecker und Velo. Die Phasen rechnet die Uhr einmal nach dem Aufwachen über die ganze Nacht, mit Normierung je Nacht, Uhrzeit-Merkmalen und Viterbi. Zuerst arbeitet sie mit den b25-Regeln, später mit einem Baum, der am PC trainiert wird.

**Besser als heute:**
- **Phasen:**
  - Stufe A (Regeln): Ziel ≥58 % und κ ≥0,35.
  - Stufe B (trainiertes Modell und RR-Nächte): Ziel ≥63 % und κ ≥0,42.

  Das sind Ziele, nicht belegt.
- Stufe A wird nur abgenommen, wenn mindestens 98 % der Minuten gleich sind wie in b25. So geht Bewährtes nicht verloren.
- **Die Messdichte in der Nacht ist wählbar:**
  - G3 wie heute (Vorgabe)
  - G2: 5 von 10 min, SpO2 in den Pausen
  - G1: durchgehend

  G2 und G1 kommen erst nach einer Akkumessung.
- Der Wecker richtet sich nach der Live-Phase: nie im Tiefschlaf, ausgelöst durch Bewegung oder einen Pulsanstieg.
- HRV-Status wie bei Garmin (7 Nächte gegen eine Basis über 3 Wochen), auf Wunsch auch ein Schlafwert.
- Velo und Auto werden zusätzlich aus 5-s-Merkmalen erkannt: Winkelstabilität, Vibration und Armschwung. Geglättet wird über mindestens 10 min, wie bei Move IQ. Kieselsport-Fahrten liefern beschriftete Lehrdaten.
- Nickerchen werden nur mit Pulsbestätigung erkannt und bekommen keine Phasen.
- Der Live-Zustand kommt vom Automaten aus Vorschlag 1. Der Schätzer des Plugin-Entwurfs ohne Puls schlief in der Probe 65–125 min zu früh ein.
- Telefon: 0x4001 und 0x4002 bleiben Byte für Byte gleich. Neu ist nur der Bandsatz für Eichnächte.

**Preis und Risiko:**
- Dieser Vorschlag hat die meiste neue Logik und braucht die meisten Daten: 10–20 weitere Fitbit-Nächte, für Stufe B mit RR-Werten. Mit den heutigen Fixtures lässt sich Stufe B nicht trainieren.
- Der Akkuverbrauch bei G1 und G2 ist ungemessen. G1 heisst ausserdem: nachts kein SpO2.
- Stufe B ist ein Modell und weniger erklärbar als Regeln.
- Das Band braucht 60–240 KiB Flash (3 bzw. 10 Tage). Ans Telefon geht es nur in Eichnächten; sonst wären es 17–24 KB pro Tag.
- «Garmin-Niveau» lässt sich nur gegen Fitbit prüfen, und Fitbit ist selbst ungenau.

**Aufwand:** gross. Laut Entwurf etwa 14 Arbeitstage bis zur ersten brauchbaren Fassung und etwa 24 insgesamt, dazu mindestens 4 Wochen Nächte. Auf dem Telefon kommt ein Parser für das Band dazu (etwa 400 Zeilen Kotlin).

**Stabilität:** Note 3 (Garmin-Entwurf) bzw. 4 (Plugin-Entwurf) im Gutachten. Die Hauptmängel behebt der Unterbau:
- Arbeit im Upstream-Callback
- Stillstand am Ladekabel
- Bandwechsel im Minutenpfad

Upstream-Eingriff: etwa 20–25 Zeilen in 5 Dateien. Die Schnittstelle mit 8 Funktionen könnte später als PR an coredevices gehen.

---

## Vorschlag 3: Insel – Boulder neben unverändertem Pebble

**Kernidee:** Pebble bleibt wie Upstream: Kraepelin zählt, führt die Sitzungen und schickt Tag 81 und 84. Boulder läuft als abgeschotteter Dienst daneben, mit eigenem Takt, eigener Arbeitsschlange, eigenem Ring und eigenen Dateien. Es schreibt nie in Pebbles Sitzungsliste. Stürzt Boulder-Code zweimal in 24 h ab, schaltet sich Boulder ab, und die Uhr läuft als normale Pebble weiter.

**Besser als heute:**
- Kleinster Eingriff: etwa 5 Zeilen in 4 Upstream-Dateien. Heute ändert der Fork allein im Gesundheitsteil 12 Upstream-Dateien (+428/−36, gemessen).
- Sitzungsfehler können Boulder nicht mehr treffen: Doppelblock, Lösch-Assert, Verlust beim Boot, ein Workout, das den Schlaf beendet.
- Selbstabschaltung statt Neustartschleife.
- Ein täglicher Diagnosesatz meldet den längsten Rechenschritt, den kleinsten freien Heap und die Neustarts.
- Normierung je Nacht, Uhrzeit-Vorlage, «ruhig wach» und HMM gibt es schon in Stufe 1.
- Der Schlaf ist nicht mehr an das Fenster 20–11 Uhr gebunden.
- Bewegungsblöcke (Gehen, Velo, Auto) gehen als eigener Satz ans Telefon.

**Preis und Risiko:**
- **Zwei Wahrheiten:** Die offizielle Pebble-App und Tag 84 zeigen weiter Kraepelins Schlaf. Kraepelins Geh- und Laufsitzungen kommen zurück und werden erst in Boulder umgedeutet.
- Auf der Uhr bleibt Kraepelins Schrittzahl. Velo und Auto werden nur markiert und erst in Boulder herausgerechnet.
- Etwa 13 KiB RAM sind dauerhaft belegt, das grösste Opfer der drei Vorschläge.
- 0x4001 und 0x4002 bekommen neue Fassungen, die Parser auf dem Telefon müssen neu geschrieben werden (etwa 600 Zeilen Kotlin).
- Der Lese-Haken des Entwurfs schrieb die Sitzungsliste um und hätte dabei über den Puffer hinaus schreiben können. Korrigiert ist er jetzt auf die App-Schnittstelle mit Kapazitätsangabe beschränkt; Zustand und Summe kommen über E5.

**Aufwand:** gross, etwa so viel wie b19–b25 zusammen, dazu 2 Wochen Tragen.

**Stabilität:** Note 4 im Gutachten, mit der Korrektur des Lese-Hakens 5. Upstream-Eingriff ebenfalls 5.

---

## Vergleich

Die Noten stammen aus den Gutachten für die ursprünglichen Entwürfe; 5 ist am besten.

| | 1 Nachlauf | 2 Garmin-Weg | 3 Insel |
|---|---|---|---|
| Schlafqualität | 3: wie b25, mehr geht nicht | 4, mit dichter Nachtmessung und RR-Nächten 5 | 4: Normierung und HMM, Messdichte wie heute |
| Schritte/Aktivitäten | 3: Velo aus Minutenwerten | 4: 5-s-Merkmale und HMM | 3: nur markiert, auf der Uhr Kraepelins Zahl |
| Alltag (Morgen, Anzeige, Wecker) | 4 | 3–4 | 3: zwei Wahrheiten |
| Stabilität | 4 | 3–4 | 4, korrigiert 5 |
| Upstream-Eingriff | 4: ca. 10 Zeilen, 3 Dateien | 3: ca. 20–25 Zeilen, 5 Dateien | 5: ca. 5 Zeilen, 4 Dateien |
| Aufwand (5 = klein) | 4: Tage | 2–3: Wochen und Nächte | 2: so viel wie b19–b25 |
| RAM fest | ca. 8,4 KB | ca. 4–5,5 KB | ca. 13 KiB |
| Zusätzliche Daten nötig | kaum | 10–20 Fitbit-Nächte, RR | mindestens 10 Nächte |

Alle drei brauchen zusätzlich die Weckregel (1–3 Zeilen) und den HRV-Fix (etwa 8 Zeilen in hrm_manager.c). Der Unterbau hebt vor allem Stabilität und Alltag bei 2 und 3. Das ist meine Einschätzung; die Gutachten haben es nicht benotet.

---

## Empfehlung: in Stufen – Vorschlag 1 bauen, im Rahmen von 3, später zu 2 ausbauen

0. **Messen, bevor gebaut wird.** Ein Messbau ohne b25-Schlaf läuft 3 Nächte und erfasst:
   - den längsten Callback
   - den Füllstand der KernelBG-Schlange
   - den kleinsten freien Heap
   - die RR-Lücken

   Ab sofort werden Coredumps abgeholt. Ausserdem bestimmen wir die Velo-Fehlschritte aus den Kieselsport-Fahrten in Boulders Datenbank; bisher sind sie ungemessen.
1. **V2.0:**
   - Nachlauf-Motor mit den unveränderten b25-Regeln
   - Gleichwertigkeitstest mit 0 abweichenden Minuten als Pflicht
   - Unterbau und Weckregel
   - Arbeitsschlange und Selbstabschaltung aus der Insel
   - Das Band schreibt RMSSD und Handwinkel schon mit.
2. **V2.1:** Velo und Auto, eingestellt an Kieselsport-Fahrten und an ein paar Autofahrten, die du markierst.
3. **V2.2:** die Rechnung über die ganze Nacht aus Vorschlag 2 (Normierung, Uhrzeit-Merkmale, HMM), sobald genug Fitbit-Nächte da sind. Eine neue Regel kommt nur dazu, wenn sie auf getrennten Nächten gewinnt.

**Warum:**
- Was dir gefallen hat, bleibt nachweisbar gleich, und die Absturzquellen sind trotzdem weg.
- Beide Gutachten kommen unabhängig voneinander auf diese Mischung: Rahmen der Insel und Nachlauf-Motor für die Stabilität, Garmin-Inhalt als Ziel für die Qualität.
- Der Eingriff bleibt klein: laut Gutachten etwa 8–10 Upstream-Zeilen in 4 Dateien. Kraepelin wird nur an einer Stelle (E3) berührt.
- Bessere Phasen kommen nicht aus mehr Code, sondern aus dichterer Pulsmessung in der Nacht und aus belegten Nächten. Deshalb kommt der Qualitätsausbau erst, wenn die Daten da sind.

---

## Was du entscheiden musst

1. **Nachtmessung und Akku:** Bleibt die Pulsmessung nachts wie heute, oder darf sie dichter werden: G2 (5 von 10 min, SpO2 in den Pausen) oder G1 (durchgehend, dafür nachts kein SpO2)? Ohne dichtere RR-Messung bleiben die Phasen etwa auf dem heutigen Stand. Mein Vorschlag: je Stufe 3 Nächte den Akku messen, dann entscheiden. Als Rahmen hattest du 14 statt 30 Tage Laufzeit akzeptiert.
2. **Fitbit-Nächte:** Trägst du noch 10–20 Nächte den Fitbit mit, wenn möglich mit Datenexport statt Bildern? Ohne diese Nächte lässt sich eine Verbesserung nur behaupten, nicht belegen.
3. **Velo- und Autoschritte:** Sollen sie von der Tagessumme abgezogen werden (die Zahl fällt dann sichtbar zurück, Uhr und App zählen gleich)? Oder nur markiert (Kraepelins Zahl bleibt auf der Uhr, Boulder rechnet sie heraus)? Und sollen Velofahrten ohne gestartetes Training in Boulder als «Bewegung» erscheinen, als Ersatz für Typ 8?
4. **Nickerchen:** Sollen sie erkannt werden (15 min bis 3 h, ohne Phasen, nur mit Pulsbestätigung), oder lassen wir sie weg? Pebble zählt Nickerchen zur Tagessumme Schlaf. Ein falsch erkanntes Nickerchen auf dem Sofa macht die TimeStyle-Zahl falsch.
5. **Sicherheitsnetz:** Darf sich Boulder selbst abschalten, wenn eigener Code innerhalb von 24 h zweimal abstürzt? Und soll die Boulder-App nach jedem Neustart den Coredump abholen?

**Kann warten:**
- Upstream-PRs an coredevices (öffentlich, nur mit deiner Zustimmung)
- Schlafwert 0–100
- Pebbles Geh-Meldungen (Insights)
- ob nachts eine vorläufige Schlafsumme oder «–» angezeigt wird
Shell cwd was reset to /home/dysseus/Documents/Claude
~~~~
