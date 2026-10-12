# Auszüge: `boulder-v1/correlation-konzept.md`

KEINE vollständige Datei. Die Datei selbst wurde von Workflow-Agenten geschrieben, deren Schreibvorgänge nicht im Transkript stehen. Hier stehen nur Befehle der alten Sitzung (Sitzung `session_011udJ51Jg5Y9cBcFvGfK26z`), die Teile davon gelesen haben, mit ihrer Ausgabe, wörtlich. Ausgaben können auch andere Dateien betreffen.

## 2026-10-08 16:55:53 (toolu_01FUtTsTWbDAyDzXa9t3Mswv)

Befehl:

~~~~
python3 - <<'EOF'
import json
d0 = json.load(open('/tmp/claude-1000/-home-dysseus-Documents-Claude/a0d87cde-04f6-43b4-bc17-a2150aed9501/tasks/wbqffdifk.output'))
d = d0['result']
if isinstance(d, str): d = json.loads(d)
a = d['audit']; i = a.find('**Was beim Entfernen'); print(a[i:i+1800])
print('\n== KONZEPT'); print(str(d['konzept'])[:3500])
print('\n== AUFWAND'); print(str(d['aufwand'])[:3500])
EOF
ls ~/Documents/Claude/boulder-v1/entwurf-correlation/ 2>/dev/null
~~~~

Ausgabe:

~~~~
**Was beim Entfernen der Trends betroffen ist**
- **Sechs Einstiege:**
  - Steps-Woche „Trends >" (dort ist auch die ganze Karte tippbar)
  - „Trends >" auf der Nachtseite
  - die letzte Karte auf Heart & Body, Water und Stack
  - „Trends >" bei Sport › This week
- **Ganz weg:**
  - die zwei Routen samt ihren Screens und iOS-Stubs
  - die drei Ansichtsdateien (`TrendAnsicht.kt`, `Trendbilder.kt`, `Vergleiche.kt`)
  - die Trend-Strings
  - vier Tests müssen angepasst werden
- **Bleibt:** die Tagesdaten (`Trenddaten`, `Tagesspeicher`, `Spalte`). Die Sicherung braucht sie, und sie sind die Grundlage für die neue Seite „Korrelation".
- **Erst nach dem Korrelations-Entwurf löschen:** den Gruppenvergleich (`Auswertung.kt`, `Vergleichsfragen.kt`). „Magnesium gegen kein Magnesium" ist genau dieser Vergleich.
- **Stack ohne Uhr-Apps:** Die Seite wäre danach ganz leer und braucht einen Leerzustand.
- **Ohne Trends hat die HRV keine eigene Farbe mehr.** Die Korrelation-Seite bräuchte sie wieder.

**Entscheid nötig:** Gehen die Sport-Trends (Eight weeks, By sport, Four weeks) mit? Diese Ansichten gibt es sonst nirgends in der App.

Die vollständige Liste steht im Bericht, mit Bild und Datei:Zeile zu jedem Befund und der Tabelle „Abweichung → richtig wäre". Den Aufwand für die Korrelation-Seite schätzt dieser Bericht noch nicht.

Dateien in /home/dysseus/Documents/Claude/boulder-v1:
- design-pruefung-0.52.md
- design-pruefung-0.52/entwurf-Trends.png
- design-pruefung-0.52/entwurf-Trenddetail.png
- design-pruefung-0.52/entwurf-Herz.png

== KONZEPT
Die Seite **Correlation** ist ausgearbeitet: ein Konzept mit Aufwandschätzung und sechs Entwürfe. Die sechs Entwürfe sind mit Firefox headless und einem eigenen Ersatz der DC-Laufzeit gerendert und angesehen; in der echten `support.js` sind sie noch nicht gesehen. Im Boulder-Repo ist nichts geändert. Einen Worktree habe ich nicht angelegt, weil keine Test-Dateien nötig waren; es gibt also nichts aufzuräumen.

**Konzept** (`correlation-konzept.md`)
- **Was wegfällt:** alle Health-Trends mit ihren sechs Einstiegen und die neun festen Vergleichsfragen.
- **Was bleibt:** `Trenddaten`, `Tagesspeicher`, `Spalte`, `Auswertung.vergleich` und `Farben.hrv`.
- **Drei Kartentypen:**
  - **Timeline:** zwei Spuren auf einer Zeitachse, 24 h oder 7 Tage. Jede Spur hat ihre Achse rechts, Nacht schattiert, ein Fadenkreuz durch beide. Keine Doppelachse. Das ist dein Beispiel Koffein + Puls.
  - **Compare:** Tage mit gegen ohne, oder vorher gegen nachher. Die Zeitrichtung ist automatisch, ein Abend zählt für die Nacht danach. Das ist dein Beispiel Magnesium ja/nein.
  - **Day by day:** zwei Tagesreihen, die zweite um eine Nacht verschoben.
- **Werte-Matrix:** Für jeden Wert stehen Quelle, Auflösung, wie weit er zurückreicht, Kartentyp und Mindestmenge. Puls je Minute, Schritte und SpO2 reichen nur 90 Tage zurück.
- **Statistik:** Median je Gruppe, Abstand, Gruppengrösse, mindestens 5 je Gruppe. Unklares bleibt grau mit „no difference“. Kein r, kein p, keine Wörter wie „because“.
- **Ablauf:**
  - „New card“ ist eine Seite in vier Schritten mit Vorschau. Der Name der Karte entsteht automatisch.
  - Ein langer Druck öffnet ein Blatt mit Move up, Move down, Edit und Delete.
  - Höchstens 12 Karten.
  - Der Leerzustand zeigt bis zu drei Vorschläge.
- **Speicherung:** JSON in eigenen Prefs (`boulder-korrelation`), keine neue Datenbanktabelle.
- **Navigation:** eine neue Zeile „Correlation“ am Ende der Health-Seite. Kein eigener Reiter, die Leiste bleibt bei 5 Einträgen.
- **Aufwand:** etwa 12,5 Arbeitstage (11 bis 15), in drei Etappen:

| Etappe | Inhalt | Tage |
|---|---|---|
| 0.53 | Trends weg, Seite, Speicherung, Compare (Magnesium) | ~6 |
| 0.54 | Timeline 24 h / 7 Tage (Koffein + Puls) | ~4 |
| 0.55 | Day by day, Before/after, Karten in der Sicherung | ~2,5 |

  Etwa 2'000 Zeilen Kotlin und 1'200 Zeilen Tests kommen dazu. Weg fallen etwa 1'500 Zeilen, mit den Sport-Trends etwa 2'000.

**Offene Entscheide (E1–E7)**
- **E1:** Sollen die Sport-Trends auch weg? Ihre drei Bilder (EIGHT WEEKS, BY SPORT, FOUR WEEKS) gibt es sonst nirgends.
- **E2:** Welche Etappe zuerst? Wenn Koffein + Puls zuerst kommen soll, kostet das etwa einen halben Tag mehr.
- **E3:** Steht der Einstieg am Ende der Health-Seite oder als vierte Zeile unter Heart & Body, Water und Stack?
- **E4:** Kommen die Karten in die Sicherung?
- **E5:** Soll der Leerzustand Vorschläge zeigen?
- **E6:** Puls aus fremden Apps einbeziehen? Vorschlag: nein, nur die Uhr.
- **E7:** Gilt die Grenze von 12 Karten?

**Entwürfe** (412 breit, dunkel, über Links verbunden)
- **Health:** die Seite nach unten gescrollt, mit der neuen Zeile „Correlation · 5 cards“.
- **Liste:** fünf Karten, darunter eine mit „no difference“ und eine mit „needs more days“.
- **Leerzustand:** „No cards yet“ mit drei Vorschlägen.
- **New card:** klickbar mit Logik, in node für jeden Typ und jede Wahl ohne Fehler durchgelaufen.
- **Koffein + Puls 24 h** und **Magnesium ja/nein:** die beiden Beispiele als Detailseiten.

Die Diagrammdaten

== AUFWAND
Ich habe die Aufwandsschätzung und die Gegenprüfung des Konzepts geschrieben. Correlation mit allen drei Kartentypen braucht etwa 11–13 Agentenläufe, rund 4,5–5,5 h, in zwei Etappen. Am Code ist nichts geändert. Einen Worktree habe ich nicht angelegt, weil die Schätzung keine Testdateien brauchte; es gibt also keinen Zweig `screens-wegwerf` aufzuräumen.

**Massstab:** 0.51 hatte +4'742/−1'115 Zeilen mit 8 Agenten in 3,3 h, 0.52 hatte +2'032/−147 mit 6 in 2 h. Ein Lauf entspricht also etwa 400–700 geänderten Zeilen, die Wandzeit rund 0,35–0,4 h je Lauf.

**Umfang:**
- **Correlation ganz** (Typ A, B, C, Before/after, Sicherung): neu etwa 2'900 Zeilen Kotlin und 1'700 Zeilen Tests. Weg fallen etwa 1'200 Zeilen Health-Trends, mit den Sport-Trends etwa 1'600.
- **Design-Korrekturen**, nur soweit gewählt:

| Stufe | Inhalt | Läufe |
|---|---|---|
| D1 klein | Köpfe, Leerzustände, Begriffe, Kopfleiste, Löschform | 1–1,5 |
| D2 Farben | doppelte Farben trennen, Koffeinkurve mit Raster | 1–1,5 |
| D3 gross | ein Zonenmodell, eine Bauart für Säulen | 2,5–3 |

**Arbeitspakete:**

| Paket | Läufe | Risiko |
|---|---|---|
| Trends entfernen, Links umhängen | 1 (+0,3 mit Sport-Trends) | niedrig |
| Datenzugriffe je Wert | 1,5–2 | mittel–hoch |
| Laden der Zeitfenster für die Timeline | 1 | niedrig–mittel |
| Kartenmodell und Speicherung | 0,75 | niedrig |
| Editor | 1–1,25 | mittel |
| Seiten und Kartentypen A/B/C | 2,5–3 | mittel |
| Diagramme (zwei Spuren mit Fadenkreuz, Tagesspalten) | 1,5–2 | mittel–hoch |
| Statistik und Paarung | 1 | mittel |
| Bildtests, Gegenprüfung, Abschluss | 1,5 | niedrig |

**Vorschlag zur Reihenfolge:**
- **0.53, 10–11 Läufe, ~4–4,5 h:** Trends raus, Grundgerüst der Seite, Compare mit/ohne (Magnesium), Timeline 24 h (Koffein und Puls), kleine Design-Korrekturen. Das ist grösser als 0.51. Ist es zu viel, wandert die Timeline nach 0.54 und 0.53 hat ~7 Läufe, ~3 h.
- **0.54, 5–6 Läufe, ~2,5 h:** Timeline über 7 Tage, Day by day, Before/after, Karten in der Sicherung, Blutdruck und Gewicht, Farb-Korrekturen. Die Farben besser vor oder mit den Correlation-Spuren, weil diese die Farbe je Grösse übernehmen.
- **Später, nur wenn gewählt:** Zonenmodell und Säulenbauart (3–4 Läufe, ~1,5 h).

**Was am Konzept nicht stimmt** (Abschnitt „Gegenprüfung“, 9 Punkte):
- **Magnesium ohne ist mit den heutigen Daten kaum bestimmbar.** Pro Tag ist nur gespeichert, wie viele Präparate fällig und genommen waren, nicht welche. SupCycle meldet nur nach einer Einnahme, und vom Plan ist nur der aktuelle da. Wer Magnesium täglich nimmt, sieht nie eine Zahl. Abhilfe: ab 0.53 den Planverlauf in SupCycle festhalten; Funk-Verbindung und Benachrichtigungen bleiben dabei unberührt.
- **Kurpausen und Before/after hängen an der Zeit.** Jede Veränderung über die Wochen sähe wie eine Wirkung aus. Diese Karten bekommen darum nie grün oder gelb, nur neutral.
- **„Clear“ ist bei 5 Tagen je Gruppe zu locker.** Richtig wäre bei 5 + 5 Tagen eine Schwelle von 2,31 statt 2. Bei 90 Tagen Fenster sollen es mindestens 8 Tage je Gruppe sein.
- **Der Fehler ist für Mittelwerte gerechnet, die Anzeige zeigt Mediane.** Als Ergebnis darum nur gleichmässige Werte wie Ruhepuls, HRV, Schlaf oder Schritte, keine Mengen wie Koffein oder Trainingsminuten.
- **Neue Reihen gehören nicht in `Trenddaten.lies`.** Das läuft auch in der nächtlichen Sicherung. Ein eigener Lader soll nur die Werte der vorhandenen Karten lesen.
- **Einzelne Werte fallen weg oder ändern sich:**
  - „Heart ra
canvas.json
Correlation-Health.dc.html
Correlation-Koffein-Puls.dc.html
Correlation-Leer.dc.html
Correlation-Liste.dc.html
Correlation-Magnesium.dc.html
Correlation-Neu.dc.html
~~~~

## 2026-10-08 16:59:26 (toolu_01T6CirqQdPr8dZvKWVLs2fW)

Befehl:

~~~~
cd /home/dysseus/Documents/Claude/boulder-v1; wc -l correlation-aufwand.md correlation-konzept.md design-pruefung-0.52.md; grep -n "^#\|^\*\*E[0-9]\|^- \*\*E\|^E[0-9]\|^| E" correlation-konzept.md | head -60
~~~~

Ausgabe:

~~~~
  258 correlation-aufwand.md
  330 correlation-konzept.md
  351 design-pruefung-0.52.md
  939 total
1:# Correlation – Konzept
5:## Kurz
8:- **Eigene Karten:** Der Nutzer legt die Karten selbst an. Jede Karte zeigt **zwei Werte** zusammen.
19:## 1. Was wegfällt, was bleibt
36:## 2. Kartentypen
38:### A – Timeline (beide Werte über die Zeit)
69:### B – Compare (mit/ohne, vorher/nachher)
98:### C – Day by day (Verschiebung)
111:## 3. Werte und Kombinationen
113:### Matrix
140:### Regeln
149:### Tage zählen
151:- **Ein Koffeintag mit 0 mg** zählt erst ab dem ersten eigenen Glas. So rechnet heute schon `koffeinMitNull`. Davor weiss Boulder nicht, ob protokolliert wurde.
155:### Mindestmengen
164:## 4. Statistik (ehrlich und einfach)
183:## 5. Ablauf
199:- **Edit:** Der Stift oben rechts auf der Detailseite öffnet dieselbe Seite, vorbelegt, mit dem Knopf „Save“.
211:## 6. Speicherung
214:- **Eine Karte:** `{"v":1,"id":"k7f3","typ":"vergleich","a":"praeparat:Magnesium","b":"tief","teilung":"mit","stichtag":null,"fenster":30}`
217:- **Ergebnisse werden nicht gespeichert.** Sie werden beim Öffnen gerechnet, aus einem gemeinsamen Lesen für alle Karten. Ein Prozess-Cache hält sie wie `Trenddaten.zuletzt`.
220:## 7. Datenweg (Code)
231:## 8. Navigation
233:- **Einstieg auf Health:** eine neue Kasten-Zeile „Correlation“ am Seitenende, nach „Steps this week“.
237:- **Es entfallen:**
244:## 9. Design
272:## 10. Aufwand
301:## 11. Offene Entscheide
303:- **E1 – Sport-Trends** (EIGHT WEEKS, BY SPORT, FOUR WEEKS): mit entfernen?
306:- **E2 – Reihenfolge der Etappen:** Compare zuerst, oder Timeline zuerst wegen Beispiel 1?
307:- **E3 – Einstieg:** am Ende der Health-Seite (Vorschlag) oder als vierte Zeile unter Heart & Body, Water und Stack?
308:- **E4 – Karten in der Sicherung?** Vorschlag: ja.
309:- **E5 – Vorschläge im Leerzustand?** Vorschlag: ja, bis zu drei.
310:- **E6 – Puls fremder Apps** (`HeartRateRecord`) einbeziehen? Vorschlag: nein, nur die Uhr als eine Quelle.
311:- **E7 – Grenze von 12 Karten?**
313:## 12. Entwürfe
~~~~

## 2026-10-08 16:59:29 (toolu_01BcR55ymvTh4VdhcGPqTkmE)

Befehl:

~~~~
cd /home/dysseus/Documents/Claude/boulder-v1; sed -n 1,35p correlation-konzept.md; sed -n 155,185p correlation-konzept.md; sed -n 272,330p correlation-konzept.md
~~~~

Ausgabe:

~~~~
# Correlation – Konzept

Stand 08.10.2026. Grundlage: Boulder 0.52.0 (`main` 34fc13cb) und die Design-Prüfung `design-pruefung-0.52.md`. Nur Konzept und Entwürfe. Im Code ist nichts geändert.

## Kurz

- **Die Trends fallen ganz weg.** Neu gibt es die Seite **Correlation** unter Health.
- **Eigene Karten:** Der Nutzer legt die Karten selbst an. Jede Karte zeigt **zwei Werte** zusammen.
- **Drei Kartentypen:**
  - **Timeline:** beide Werte auf einer Zeitachse, über 24 Stunden oder 7 Tage.
  - **Compare:** Tage mit gegen Tage ohne, oder vorher gegen nachher.
  - **Day by day:** zwei Tagesreihen, die zweite um eine Nacht verschoben.
- **Statistik gibt es nur bei Compare.** Es zählen Median, Abstand und Gruppengrösse. Unklares bleibt grau mit „no difference“. Jede Gruppe braucht mindestens 5 Tage. Die bestehende Rechnung `Auswertung.vergleich` wird übernommen.
- **Speicherung:** Die Karten liegen als JSON in eigenen Prefs. Es braucht keine neue Datenbanktabelle.
- **Aufwand:** etwa 12 Arbeitstage (11 bis 15), in drei Etappen lieferbar (Abschnitt 10).

---

## 1. Was wegfällt, was bleibt

**Es fällt weg** (Fundstellen siehe `design-pruefung-0.52.md`, Abschnitt „Trends entfernen“):
- `TrendAnsicht`, `Trendbilder`, `Vergleiche` und `TrendRoute`.
- Die Strings `kiesel_trend_strings.xml`, `ks_tr_*` und die `ks_v_*` der Vergleiche.
- Die neun festen Fragen in `Vergleichsfragen`. Ihre Regel zur Zeitrichtung geht in die neue Rechnung über.
- **Sechs Einstiege:**
  - der Link über „Steps this week“,
  - die Mitte „Trends“ in der Nacht,
  - die Trendzeile auf Heart & Body, Water und Stack,
  - „Trends >“ auf Sport (siehe Entscheid E1).

**Es bleibt und wird zur Grundlage:**
- `Trenddaten.lies`, `Tagesspeicher`, `Spalte` und die Sicherung.
- `Auswertung.vergleich` mit `Statistik`.
- `Farben.hrv`: Die Correlation-Seite braucht die Farbe wieder.

### Mindestmengen

| Typ | Mindestens | Sonst |
|---|---|---|
| A, 24 h | Daten in der Spur | Leerzeile in der Spur („No heart rate“) |
| A, 7 d | 1 Tag je Spur | dto. |
| B | 5 je Gruppe (`VERGLEICH_MINDESTENS`) | „needs more days“, Zeilen mit „4 of 5 nights“ |
| C | 7 Tage mit beiden Werten (`RICHTUNG_MINDESTTAGE`) | „needs more days“ |

## 4. Statistik (ehrlich und einfach)

- **Die Rechnung kommt aus `Auswertung.vergleich`:**
  - Median je Gruppe.
  - Abstand = Median A − Median B.
  - Streuung = gepoolte Standardabweichung.
  - Fehler = 1,25 × Streuung × √(1/nA + 1/nB).
  - „Clear“ ab 2 Fehlern (`KLAR_AB_FEHLERN`).
  - Beispiel Magnesium: 92 gegen 74 min, Streuung ±8, Fehler 4, also klar.
- **Anzeige:**
  - Der Abstand steht in der Einheit des Ergebnisses.
  - Ist er nicht klar, steht er grau da, mit „no difference“.
  - Sind es zu wenige Tage, steht keine Zahl da, nur „needs more days“.
- **Wörter:** „with“ / „without“, „after“, „before“ / „since“. Nie „because“, „improves“ oder „causes“. Kein r, kein p, keine Prozent-Sicherheit.
- **Bewusst nicht dabei:** ein Abgleich von Störgrössen und eine Korrektur für viele Tests.
  - Folge: Bei etwa zehn Compare-Karten kann ein „clear“ Zufall sein.
  - Dagegen helfen ohne Text: die Mindestmenge 5, die Schwelle 2 Fehler und das Bild „Night by night“. Es zeigt, ob die Gruppen zeitlich getrennt liegen. Das ist die häufigste Täuschung bei Before/after.
- **Timeline und Day by day** beschreiben nur: Werte und gestrichelte Mittel.

## 5. Ablauf

**Liste:**
## 10. Aufwand

| Baustein | Tage |
|---|---|
| 1. Trends entfernen: Routen, Ansichten, Strings, 6 Einstiege, Tests, Bildquellen | 1 |
| 2. Werte-Katalog und neue Tagesreihen (Nachtpuls, SpO2 tief, REM, Koffein beim Einschlafen, Präparate je Name, Trainings, Blutdruck, Gewicht), festgeschrieben | 2,5 |
| 3. Fensterlader für Timeline: 8 Werte, 24 h und 7 d, Trainings im Fenster | 1,5 |
| 4. Rechnung für Compare, Before/after und Day by day mit Zeitrichtung; Tests mit je einer Mutation pro Kernaussage | 1,5 |
| 5. Speicherung, Reihenfolge, Vorschläge, Sicherung | 0,5 |
| 6. Oberfläche: Liste, 3 Kartenarten, 3 Detailseiten, New/Edit, Blatt, Leerzustände, Zeile auf Health, Routen und iOS-Stubs | 4 |
| 7. Bildtests aller Zustände (dunkel/hell), Wächter, Screens neu, Release-Notiz, Prüfung auf dem Telefon | 1,5 |
| **Summe** | **12,5 (Spanne 11–15)** |

**Code:** neu etwa 2'000 Zeilen Kotlin und 1'200 Zeilen Tests. Weg fallen etwa 1'500 Zeilen ohne die Sport-Trends, etwa 2'000 mit ihnen.

**Etappen:**
1. **0.53, etwa 6 Tage:** Trends weg; die Seite mit Speicherung, Leerzustand und Zeile auf Health; Compare mit/ohne, also das Magnesium-Beispiel.
2. **0.54, etwa 4 Tage:** Timeline über 24 h und 7 d, also das Beispiel Koffein und Puls.
3. **0.55, etwa 2,5 Tage:** Day by day, Before/after und die Karten in der Sicherung.

Soll Koffein und Puls zuerst kommen, tauschen Etappe 1 und 2 die Typen. Das kostet etwa einen halben Tag mehr, weil Compare dann ohne die vorbereiteten Tagesreihen startet.

**Risiken:**
- **Minutendaten nur 90 Tage** (Puls, Schritte je 30 min, SpO2). Die Timeline reicht nicht weiter zurück. Die Reihen für Nachtpuls und SpO2 werden erst ab ihrer Einführung länger.
- **Der Präparate-Verlauf** hängt an den Einträgen in der Akte. Bis 0.33 hatten sie keine eigene Kennung, darum der Abgleich über Name und Herkunft.
- **Leselast in Health Connect:** Ernährung über 90 Tage sind viele Einträge. Gelesen wird seitenweise und nur bei sichtbarer Seite.
- **7-Tage-Timeline:** etwa 10'000 Pulsminuten. Sie werden vor dem Zeichnen in 30-Minuten-Fächer gefasst.
- **Zufallstreffer** bei vielen Compare-Karten (siehe Abschnitt 4).

## 11. Offene Entscheide

- **E1 – Sport-Trends** (EIGHT WEEKS, BY SPORT, FOUR WEEKS): mit entfernen?
  - Vorschlag: ja, weil „alle Trends“ gesagt ist. Sport behält „This week“ und „Recent“.
  - Achtung: Diese drei Bilder gibt es sonst nirgends.
- **E2 – Reihenfolge der Etappen:** Compare zuerst, oder Timeline zuerst wegen Beispiel 1?
- **E3 – Einstieg:** am Ende der Health-Seite (Vorschlag) oder als vierte Zeile unter Heart & Body, Water und Stack?
- **E4 – Karten in der Sicherung?** Vorschlag: ja.
- **E5 – Vorschläge im Leerzustand?** Vorschlag: ja, bis zu drei.
- **E6 – Puls fremder Apps** (`HeartRateRecord`) einbeziehen? Vorschlag: nein, nur die Uhr als eine Quelle.
- **E7 – Grenze von 12 Karten?**

## 12. Entwürfe

Ablage: `entwurf-correlation/` (412 breit, dunkles Schema, Format wie `entwurf-erlebnis`). Dazu `canvas.json` mit den sechs Bildschirmen nebeneinander. Die Bildschirme sind über Links verbunden.

| Datei | Inhalt |
|---|---|
| `Correlation-Health.dc.html` | Health nach unten gescrollt: Steps ohne „Trends >“, neue Zeile „Correlation · 5 cards“ |
| `Correlation-Liste.dc.html` | Eigene Karten: Timeline Koffein und Puls, Compare Magnesium (+18 min, grün), Day by day Koffein beim Einschlafen und HRV, Compare Schritte (no difference, grau), Before/after Zink (needs more days), „New card“ |
| `Correlation-Leer.dc.html` | Leerzustand: „No cards yet“, drei Vorschläge, „New card“ |
| `Correlation-Neu.dc.html` | New card mit Logik: Typ wählen, Werte aus gruppierten Listen mit Tageszahl, „in use“, Fenster, Vorschau, „Add card“ |
| `Correlation-Koffein-Puls.dc.html` | Timeline 24 h: zwei Spuren, Nacht, Ruhepuls gestrichelt, Fadenkreuz 1:00 AM, Kennzahlen, Earlier/Later, „Last 7 nights“ |
| `Correlation-Magnesium.dc.html` | Compare: +18 min, zwei Gruppen, Night by night mit Einnahme-Punkten und gestrichelten Medianen, How sure |

**Geprüft:**
- Alle sechs mit Firefox (headless) gerendert und angesehen. Für diese Ansicht lief ein eigener kleiner Ersatz der DC-Laufzeit. Er liegt im Scratchpad und ist **nicht** in der Ablage.
- Die Logik von New card lief in node durch: jeder Typ, jede Wahl, jedes Fenster, ohne Fehler.
- **In der echten Laufzeit noch nicht gesehen:** Die echte `support.js` kommt erst beim Veröffentlichen dazu.
- Die Diagrammdaten sind gestellt, aber in sich stimmig. Nachtpuls 57, Koffein beim Einschlafen 63 mg und die Magnesium-Mediane 92 gegen 74 min sind aus den gezeichneten Werten gerechnet.
~~~~
