# Auszüge: `boulder-v1/correlation-aufwand.md`

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

## 2026-10-08 16:59:33 (toolu_014JrJ1iyuchCmEQpjh9BnXv)

Befehl:

~~~~
cd /home/dysseus/Documents/Claude/boulder-v1; sed -n 1,60p correlation-aufwand.md; grep -n "^#" correlation-aufwand.md
~~~~

Ausgabe:

~~~~
# Correlation – Aufwand und Gegenprüfung

Stand 08.10.2026, `main` 34fc13cb (0.52.0). Grundlage: `correlation-konzept.md`, `design-pruefung-0.52.md`, Code in `~/Projects/Boulder`. Nichts geändert, nichts committet. Kein Worktree angelegt: für die Schätzung waren keine Testdateien nötig, es gibt also keinen Zweig `screens-wegwerf` aufzuräumen.

Kürzel wie in der Design-Prüfung: **K** = `pebble/src/androidMain/kotlin/coredevices/pebble/kiesel`, **A** = `K/ansicht`, **UA/UC** = ui android/common, **T** = Hosttests.

## Kurz

- **Correlation ganz (A, B, C, Before/after, Sicherung):** etwa **11–13 Agentenläufe**, rund **4,5–5,5 h** Wandzeit in zwei Etappen. Neu ~2'900 Zeilen Kotlin und ~1'700 Zeilen Tests; weg ~1'200 Zeilen (Health-Trends), mit Sport-Trends ~1'600.
- **Design-Korrekturen** sind getrennt geschätzt: klein ~1,5 Läufe, Farben ~1,5, gross (Zonen, Säulen) ~3. Sie kommen nur, soweit gewählt.
- **Vorschlag:**
  - **0.53:** Trends raus, Gerüst, **B with/without** (Magnesium), **A 24 h** (Koffein und Puls), kleine Design-Korrekturen. Etwa **10–11 Läufe, ~4–4,5 h**.
  - **0.54:** A über 7 Tage, C, Before/after, Karten in der Sicherung, Farb-Korrekturen. Etwa **5–6 Läufe, ~2,5 h**.
  - **Später, nur wenn gewählt:** ein Zonenmodell, eine Säulenbauart.
- **Das Konzept braucht 9 Korrekturen** (Abschnitt „Gegenprüfung“). Die wichtigsten:
  - „Not taken“ bei Präparaten ist mit den heutigen Daten nicht sauber bestimmbar.
  - Before/after und Kurpausen sind zeitlich verwechselt.
  - Bei 5 Tagen je Gruppe ist „clear“ zu locker.
  - REM und Wach sind derzeit falsch gemessen.
  - Neue Reihen gehören nicht in `Trenddaten.lies`, sonst wird die nächtliche Sicherung schwer.

---

## Massstab

| Fassung | Agenten | Wandzeit | Zeilen (pebble + libpebble3) |
|---|---|---|---|
| 0.51 | 8 | ≈ 3,3 h | +4'742 / −1'115, 52 Dateien |
| 0.52 | 6 | ≈ 2 h | +2'032 / −147, 38 Dateien |

- **Ein Agentenlauf** sind etwa 400–700 geänderte Zeilen samt Tests und Bildtests, oder eine Gegenprüfung beziehungsweise ein Abschluss.
- **Wandzeit:** rund 0,35–0,4 h je Lauf, weil Läufe in Wellen parallel gehen. Die Kette Daten → Rechnung → Oberfläche → Bildtests braucht mindestens drei Wellen.
- **Gestrichener Code** kostet wenig. Teuer sind neue Bilder mit Gesten und Rechnungen, die stimmen müssen.

---

## Arbeitspakete

| # | Paket | Umfang (grob) | Risiko | Abhängig von | Läufe |
|---|---|---|---|---|---|
| P1 | Trends entfernen, Links umhängen | −1'200 Z.; ~14 Dateien angepasst (5 Ansichten, 3 Screens/Routen, iOS-Stub, 4 Tests, Bildquelle, Strings) | niedrig | – | 1 (+0,3 mit Sport-Trends) |
| P2 | Datenzugriffe je Wert (Tagesreihen) | ~450 Z. + 350 Test; neu `K/Korrelationsdaten.kt`, Änderung `SupCycle.kt`, leichter Trainingsleser | **mittel–hoch** | – | 1,5–2 |
| P3 | Fensterlader Timeline (24 h / 7 d) | ~250 Z. + 200 Test; über `Gesundheitsquelle` | niedrig–mittel | – | 1 |
| P4 | Kartenmodell und Speicherung | ~200 Z. + 150 Test; `K/Korrelationskarten.kt`, kotlinx.serialization | niedrig | – | 0,75 |
| P5 | Editor New/Edit | ~350 Z. + Bildtest | mittel | P2, P4 | 1–1,25 |
| P6 | Seiten und Kartentypen A/B/C (ohne Bilder) | ~700 Z.: Liste, Blatt, Leerzustand, Health-Zeile, Routen, 3 Detailseiten | mittel | P4, P8 | 2,5–3 |
| P7 | Diagramme | ~600 Z.: `Spurenbild` (2 Spuren, Fadenkreuz, 8 Spurarten), `Tagesspalten` | **mittel–hoch** | P3 | 1,5–2 |
| P8 | Statistik und Paarung | ~200 Z. + 250 Test; `K/Korrelation.kt`, `Auswertung.vergleich` mit Fenster | mittel (Korrektheit) | P2 | 1 |
| P9 | Bildtests, Wächter, Gegenprüfung, Abschluss | ~600 Z. Bildtests; Screens neu, Release-Notiz | niedrig | alle | 1,5 |
| **Σ Correlation** | | **~2'900 + ~1'700 Test** | | | **11–13** |
| D1 | Design klein | Strings und Layout, ~15 Dateien | niedrig | P1 | 1–1,5 |
| D2 | Design Farben | `Farben.kt`, Training, Sportfeld, Koffeinkurve; viele Bilder verschieben sich | mittel | – | 1–1,5 |
| D3 | Design gross | ein Zonenmodell (M1), eine Säulenbauart (M2) | mittel–hoch | D2 | 2,5–3 |

### P1 – Trends entfernen
- Fundstellen stehen vollständig in `design-pruefung-0.52.md`, Abschnitt „Trends entfernen“. Sie sind am Code geprüft und stimmen.
- **Fällt weg:**
  - die Ansichten A/TrendAnsicht (345 Z.), A/Trendbilder (457) und A/Vergleiche (313),
  - `TrendRoute` und `TrendScreen` (expect/actual/iOS),
  - die Datei `kiesel_trend_strings.xml` sowie die Strings `ks_tr_*` und `ks_v_*` (ohne die von `kiesel_auswertung_strings.xml`),
1:# Correlation – Aufwand und Gegenprüfung
7:## Kurz
24:## Massstab
37:## Arbeitspakete
55:### P1 – Trends entfernen
67:### P2 – Datenzugriffe je Wert
78:### P3 – Fensterlader Timeline
86:### P4 – Kartenmodell und Speicherung
91:### P5 – Editor
95:### P6 – Seiten und Karten
104:### P7 – Diagramme
113:### P8 – Statistik und Paarung
119:### P9 – Abschluss
124:### D1–D3 – Design-Korrekturen (aus `design-pruefung-0.52.md`)
148:## Summe und Reihenfolge
167:## Gegenprüfung (Korrekturen am Konzept)
252:## Offene Entscheide (ergänzt)
~~~~

## 2026-10-08 16:59:36 (toolu_01WvTk2jKW3GWzeCWQmKrLh3)

Befehl:

~~~~
cd /home/dysseus/Documents/Claude/boulder-v1; sed -n 124,258p correlation-aufwand.md
~~~~

Ausgabe:

~~~~
### D1–D3 – Design-Korrekturen (aus `design-pruefung-0.52.md`)
- **D1 klein (1–1,5):**
  - M3: Pulskarte mit Kopf „HEART RATE · last 24 hours“, ohne Legende.
  - M4: Köpfe „GRÖSSE · last N days“.
  - M8: Leerzustände, ein Wortlaut.
  - M9: „Resting HR“, „HRV“.
  - M10: Kopfleiste.
  - M11: Lücke auf Heart & Body.
  - M12: eine Löschform.
  - N2: „about“.
  - N5: Log.
  - M5, M6 und M7 erledigen sich mit P1.
- **D2 Farben (1–1,5):**
  - H3: Body eigene Farbe, Akku neutral, Wach eigener Ton, Urteils- von Zonenfarben trennen, Trainingspuls herzrot, Höhe neutral, Sportfeld in der Farbe der letzten Art.
  - M13: Koffeinkurve mit Raster und Achse.
  - **Sollte vor oder mit Correlation kommen**, weil die Correlation-Spuren die Farbe je Grösse übernehmen.
- **D3 gross (2,5–3):**
  - M1: ein Zonenmodell für Herzseite und Training. Achtung: Es ändert auch die Rechnung „Zeit in Zonen“.
  - M2: eine Säulenbauart für Steps, Water, Last nights und Sport.
  - Hohe Bildtest-Last.
- **Getrennt (je ~0,3–0,5):** M9 Sportnamen, N3 Widgetfarben (RemoteViews/XML), N4 Kartenlink.

---

## Summe und Reihenfolge

| Etappe | Inhalt | Läufe | Wandzeit |
|---|---|---|---|
| **0.53** | P1, P4, P2 (für B und A), P3 (24 h), P8 (ohne Before/after), P5, P6 (Liste, B, A), P7 (Spurenbild 24 h, Night by night), P9, D1 | 10–11 | ~4–4,5 h |
| **0.54** | A über 7 Tage, C, Before/after, Sicherung, Rest P2 (Blutdruck, Gewicht), D2 | 5–6 | ~2,5 h |
| später | D3 und Einzelpunkte, nur wenn gewählt | 3–4 | ~1,5 h |

- **Wellen in 0.53:**
  1. P1 ∥ P2 ∥ P3 ∥ P4
  2. P8 ∥ P7
  3. P5 ∥ P6
  4. P9 und Gegenprüfung
- **0.53 ist grösser als 0.51.** Ist das zu viel, wandert A 24 h nach 0.54. Dann trägt 0.53 nur das Magnesium-Beispiel (~7 Läufe, ~3 h).
- **D2 vor A/B** spart ein zweites Nachziehen der Spurfarben. Sonst kommt D2 in 0.54 und die Spuren nehmen die Farben, wie sie in `Farben.kt` stehen.
- **Gegen das Konzept** („12,5 Arbeitstage“): Die Zeilenzahl ist ähnlich (Konzept ~2'000 + 1'200, hier ~2'900 + 1'700). Der Unterschied kommt aus G1, G5 und dem Fadenkreuz.

---

## Gegenprüfung (Korrekturen am Konzept)

**G1 – „Not taken“ bei Präparaten ist so nicht bestimmbar.**
- **Befund:**
  - Die Quote speichert je Tag nur zwei Zahlen „fällig:genommen“ (`SupCycle.merkeQuote`, K/SupCycle.kt:379). Welches Präparat fällig war, steht nicht da.
  - SupCycle meldet nur **nach einer Einnahme** (K/SupCycle.kt:44). Ein Tag ganz ohne Haken hat also gar keine Quote, und genau das sind typische „without“-Tage.
  - Gespeichert ist nur der **aktuelle** Plan (`gespeicherterPlan`), kein Verlauf.
- **Folge:** Das Konzept zählt nur Tage mit Quote. Damit fehlen die vergessenen Tage, und „without“ besteht fast nur aus Tagen, an denen andere Präparate genommen wurden.
  - Wer Magnesium täglich nimmt, bekommt nie 5 Tage „without“, die Karte bleibt immer bei „needs more days“.
- **Korrektur:**
  - **Ab 0.53 den Planverlauf festhalten:** bei jeder Planänderung (gültig ab, Plan, Fassung). Damit ist „fällig“ für jeden Tag nachrechenbar (`faelligAm`).
  - **„With“** = Eintrag mit dem Namen an dem Tag.
  - **„Without“** = kein Eintrag an einem Tag mit Uhrdaten (getragen), an dem der Plan das Präparat kennt.
  - **Vor dem Planverlauf:** nur Tage mit Quote, wie im Konzept.
  - **„With“ heisst abgehakt, nicht geschluckt.** Das Label bleibt „with/without“.
  - **Zusätzlich** zeigt die Auswahl die Tageszahl beider Gruppen („22 / 3 days“). So ist vor dem Anlegen sichtbar, dass es ohne Pausen nicht geht.

**G2 – Before/after und Kurpausen sind zeitlich verwechselt.**
- Kurpausen liegen als ganze Wochen am Stück. „Without“ ist dann in Wahrheit ein Vorher/Nachher.
- Jede Drift (Jahreszeit, Training, Krankheit, Uhrwechsel, die Schlafkorrektur b27) erscheint als Wirkung. Night by night zeigt das nur, wenn man es zu lesen weiss, und Erklärtext ist nicht erlaubt.
- **Korrektur:**
  - Bei **Before/after** steht nie gut oder warnung, nur neutral, auch wenn „clear“.
  - **Bei with/without:** Bilden die Tage einer Gruppe höchstens 2 zusammenhängende Blöcke, gilt dasselbe. Das sind ~20 Zeilen in P8 und ohne Text.
- **Before/after nie über eine Messgrenze:** nicht über `eigenAb` (Kiesel-Helper-Vorgeschichte mass anderes, K/Trenddaten.kt:107) und nicht über das Datum einer Firmware-Fassung mit geänderter Schlafrechnung.

**G3 – „Clear“ ab 2 Fehlern ist bei 5 Tagen zu locker.**
- Bei 5 + 5 Tagen liegt die 95-%-Grenze der t-Verteilung bei 2,31, nicht bei 2. Dazu sind Nachtwerte von Nacht zu Nacht verbunden (HRV, Ruhepuls), das senkt die wirkliche Zahl der Tage.
- Mit ~10 Karten ist ein falsches „clear“ fast sicher.
- **Korrektur:**
  - Die Schwelle ist der t-Wert für nA+nB−2 Freiheitsgrade, eine kleine Tabelle, ab 30 gleich 2.
  - Die Mindestmenge steigt bei Fenster 90 auf 8 je Gruppe, bei 30 bleibt sie 5.
  - **Kein Mehrfachtest-Ausgleich** (wie Konzept). Die Zahl der Karten ist ohnehin auf 12 begrenzt.

**G4 – Die Statistik misst Mediane mit einem Fehler für Mittelwerte.**
- `MEDIAN_STREUT = 1,25` gilt nur für normalverteilte Werte. Bei schiefen Grössen (Koffein mg, Trainingsminuten, Blutdruck mit wenigen Messungen) ist der Fehler ungenau.
- **Korrektur:** Als Ergebnis nur Pegel zulassen (Ruhepuls, HRV, Schlaf, Tief, Schritte, Wasser, SpO2), keine Ereignismengen. Koffein mg und Trainingsminuten bleiben Bedingung oder C-Spur. Das verkleinert auch die Matrix.

**G5 – Neue Reihen gehören nicht in `Trenddaten.lies`.**
- `Trenddaten.lies` läuft auch in der **nächtlichen Sicherung** (K/Sicherung.kt:253) und schreibt fest.
- Mit Einträgen je Getränk über 90 Tage, Pulsminuten je Nacht (bis 130'000 Zeilen über 90 Tage) und Blutdruck würde jede Sicherung schwer, auch ohne eine einzige Karte.
- **Korrektur:**
  - Ein eigener `Korrelationsdaten`-Lader liest **nur die Werte der vorhandenen Karten**, nur bei sichtbarer Seite.
  - Festgeschrieben wird nur, was die Minutendaten überleben muss (SpO2 je Nacht). Es wird **schrittweise** geschrieben, nur Nächte, die im Archiv fehlen, und mit der Regel `SICHER_TAGE` (die Spalte gehört in `MINUTENREIHEN`, sonst schreibt eine angeschnittene Nacht über die ganze).

**G6 – Einige Werte sind falsch angenommen.**
- **„Heart rate, night“ streichen.** Es gibt schon den Ruhepuls der Firmware. Ein zweiter Nachtpuls aus Minuten wäre der fünfte Name für fast dasselbe (Design-Prüfung M9) und kostet die teure Minutenrechnung.
- **SpO2 „lowest“** hängt an der Zahl der Messungen: mehr Messungen ergeben einen tieferen Wert. Korrektur: **Median je Nacht** als Tageswert, das Minimum nur in der Timeline.
- **REM und Wach zurückhalten,** bis der Schlafvergleich (b27/V2) da ist. Heute zählt die Uhr REM teils als wach (0.53-inputs, Punkt 3: 70 min).
  - „Sleep“ ist die effektive Summe und deshalb zu tief.
  - Nach dem Entscheid vom 08.10. zählt die **Nachtlänge**. Sie kommt als eigener Wert „Night length“ dazu und gilt als Vorgabe für „Night under 7 h“.
- **Blutdruck als Ergebnis** kommt selten auf 5 Tage je Gruppe (Manschette, wenige Messungen). Er bleibt im Katalog, die Tageszahl in der Auswahl zeigt es.
- **Koffein spät und Koffein beim Einschlafen** brauchen Einträge je Getränk, nicht das Tagesaggregat. Das Konzept führt sie als Spalten wie die anderen; sie kosten aber das Lesen in P2.
- **Puls „1 min“:** Das ist die Höchstauflösung. Die Dichte hängt an der Messrate der Uhr. Lücken über 15 min brechen die Linie wie im Tagesband (Tagesbandrechnung.pulslinie).

**G7 – Die Kennzahlen „je Wertepaar fest“ passen nicht zu frei gebauten Karten.**
- Bei ~14 Timeline-Werten sind das ~90 Paare, und jedes bräuchte eigene Kennzahlen. Das ist nicht machbar und nicht prüfbar.
- **Korrektur:** Die Kennzahlen gehören **zur Spur**:
  - Puls: Schnitt und tiefster.
  - Koffein: Summe und „at sleep“.
  - Schritte: Summe.
  - Schlaf: Länge.
  - …
- Zwei Spuren geben höchstens vier Zahlen. „Nachtpuls gegen üblich“ entfällt mit G6.

**G8 – Kleinere Punkte.**
- **Das Koffeinmodell** steigt ohne Anflutung sofort auf die volle Dosis. Die Halbwertszeit ist fest 5 h (`KOFFEIN_HALBWERT_MIN`, K/Gesundheitsrechnung.kt:217). Das bleibt so, aus einer Quelle. Eine wählbare Halbwertszeit wäre ein eigener Wunsch.
- **„Earlier“ in der Timeline** endet bei 89 Tagen (Housekeeping). Danach ist der Knopf grau, er springt nicht in leere Fenster.
- **„Weekend“ bei Nachtwerten** meint die Nächte mit Aufwachen am Samstag oder Sonntag, also Fr→Sa und Sa→So. Das Label heisst darum „Weekend nights“.
- **Gleiche Farben** zweier Spuren hängen an H3. Ohne D2 sind HRV und Body (beide Violett) eine häufige Kollision.
- **Der Titel „Correlation“** ist der Wunsch des Nutzers. Die Seite zeigt bewusst keinen Korrelationswert. Das ist kein Widerspruch im Bild, nur im Wort, und bleibt so.

**G9 – Konzept gegen Grundsätze.**
- **„Nicht gewählt bleibt“:**
  - Das Konzept schlägt vor, die Sport-Trends mitzuentfernen (E1). Der Nutzer sprach von „den Trends“, gezeigt waren die Health-Trends.
  - Die Sport-Trends tragen die einzigen Bilder EIGHT WEEKS, BY SPORT und FOUR WEEKS.
  - Bis zu einer ausdrücklichen Wahl bleiben sie. Die Schätzung führt sie als +0,3 Läufe.
- **„Verbindung nicht anfassen“:** gewahrt. Die Seite liest nur Datenbank und Akte, die Änderung in `SupCycle` (G1) schreibt nur einen Prefs-Eintrag mehr. An BLE und Benachrichtigungen ändert sich nichts.
- **„Keine Textwände“:** gewahrt, wenn „How sure“ nur Zahlen und Labels trägt. Die Wörter „clear“ und „unclear“ stehen ohne Satz. G2 und G3 kommen ganz ohne Text aus.
- **„Echtheit vor Release“:**
  - Die Entwürfe sind mit gestellten Daten gerechnet.
  - Vor 0.53 muss einmal auf dem Telefon gezählt werden, wie viele Magnesium-Tage es mit und ohne gibt. Dazu: SupCycle-Prefs ansehen, Akte → Ernährung.
  - Erst dann ist klar, ob das Beispiel 2 überhaupt eine Zahl zeigen kann.

---

## Offene Entscheide (ergänzt)

- **E1** Sport-Trends: bleiben, bis gewählt (G9).
- **E2** Reihenfolge: Vorschlag 0.53 A 24 h + B, oder nur B, wenn 0.53 kleiner sein soll.
- **E8 (neu)** Planverlauf in SupCycle ab 0.53 festhalten (G1). Empfehlung: ja, sonst kann Magnesium gegen ohne kaum je eine Zahl zeigen.
- **E9 (neu)** REM, Wach und „Sleep“ erst nach dem Schlafvergleich anbieten, bis dahin „Night length“ (G6).
- **E10 (neu)** Welche Design-Korrekturen: D1, D2, D3 einzeln wählen.
~~~~
