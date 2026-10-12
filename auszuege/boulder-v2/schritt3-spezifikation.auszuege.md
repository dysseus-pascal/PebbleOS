# Auszüge: `boulder-v2/schritt3-spezifikation.md`

KEINE vollständige Datei. Die Datei selbst wurde von Workflow-Agenten geschrieben, deren Schreibvorgänge nicht im Transkript stehen. Hier stehen nur Befehle der alten Sitzung (Sitzung `session_011udJ51Jg5Y9cBcFvGfK26z`), die Teile davon gelesen haben, mit ihrer Ausgabe, wörtlich. Ausgaben können auch andere Dateien betreffen.

## 2026-10-08 09:01:30 (toolu_017B7E31fjY8H3mPPCXnQY9o)

Befehl:

~~~~
python3 - <<'EOF'
import json
d0 = json.load(open('/tmp/claude-1000/-home-dysseus-Documents-Claude/a0d87cde-04f6-43b4-bc17-a2150aed9501/tasks/wp0kmawic.output'))
d = d0['result']
if isinstance(d, str): d = json.loads(d)
print('== SPEZ'); print(str(d['spez'])[:3500])
print('\n== PRUEFUNG'); print(str(d['pruefung'])[:2000])
EOF
grep -n -E '^## |^### |^\*\*F[0-9]+' ~/Documents/Claude/boulder-v2/schritt3-spezifikation.md | head -60
~~~~

Ausgabe:

~~~~
== SPEZ
Die Spezifikation für Schritt 3 liegt in `/home/dysseus/Documents/Claude/boulder-v2/schritt3-spezifikation.md`. Sie ist ein Plan: nichts gebaut, beide Repos unverändert.

- **Bildschirme:** fünf Karten wie bei Pebble, Wechsel mit Oben/Unten oder Wischen, Select öffnet ein Detail:
  - **Today:** Schlaf, Ruhepuls, HRV und Schritte, je mit Urteil nach Boulders Regeln.
  - **Night:** Dauer, Urteil, Zeiten, die Nacht als ein Block mit Phasen, Minuten je Phase.
  - **Heart:** Puls jetzt, Pulsband über 24 h, Ruhepuls, HRV, SpO2.
  - **Activity:** Schritte gegen 10 000, km, aktive Minuten, Woche.
  - **Settings:** Precision.
  - Details: Tagesband, Nachtansicht, Zonen/SpO2/HRV, Sitzungen des Tages. Pins öffnen Night bzw. Activity.
- **Schnittstelle:** 4 eigene Syscalls (letzte Nacht, Lage, SpO2, Messstufe setzen), ein fünfter nur mit F2.
- **Pebble-Code:** genau eine neue Stelle (`fw/apps/system/CMakeLists.txt`, 4 Zeilen). Die App übernimmt ID −82 und UUID und erbt damit alle Einstiege.
- **Umfang:** 9 Arbeitspakete, kritischer Pfad AP0 → AP1 → AP2 → AP5 → AP7 → AP8. Dazu 8 Abnahmekriterien als Entwurf, 14 Vorgaben (V-Liste) zum Bestätigen.
- **Neu gefunden:**
  - Ein Minutensatz hat 12 statt 16 B, 24 h sind also 17 280 B.
  - Die Namen der Messstufen liegen im Kernel-RAM; die App darf sie nicht direkt lesen.
  - Zwei Funktionen der Variante (letzte Nacht, Messstufe) lesen beim ersten Aufruf eine Datei auf dem Stapel des Aufrufers. Die Syscalls übergeben das deshalb an KernelBG.
- **Für die Vorabversion zusätzlich nötig:**
  - Upstream v4.40.0 nachziehen; dort ist MPU- und Syscall-Code umgebaut, daher Probe ohne Systemrechte danach wiederholen.
  - Wechsel b26 → V2: die Tests liefen bisher mit einem b25-Abbild.
  - Boulder-Gegenstück: Boulder 0.52 hält jede Firmware ab b24 für Bedrock-fähig und muss V2 erkennen.
  - Kieselstraße (F6).
- **Wichtigste offene Fragen:**
  - **F2:** Nächteliste auf der Uhr (30 Nächte)? Ohne sie hat HRV kein Urteil und das Ruhepuls-Urteil ist nur ungefähr. Empfehlung: ja.
  - **F1:** Karten statt Liste (Empfehlung: Karten).
  - **F3:** Tagesband als Detail von Today (Empfehlung: ja).
  - **F4:** Precision auch in der Health-App setzen (Empfehlung: ja).
  - **F5:** Werte vom Telefon erst in Schritt 4 (Empfehlung: Schritt 4).
  - **F6:** Kieselstraße vor der Vorabversion (Empfehlung: ja).

== PRUEFUNG
Ich habe die Spezifikation gegen `origin/v2` `836b406e7` und Boulder 0.52 geprüft, die Fehler direkt in `/home/dysseus/Documents/Claude/boulder-v2/schritt3-spezifikation.md` korrigiert und unten den Abschnitt „Gegenprüfung“ angehängt (G1–G20, dazu „Geprüft und richtig“ und „Offen“). Nichts gebaut. pebbleos und Boulder sind unverändert (`git status` leer). Das Archiv in `s3-bestand` ist gleich `git archive 836b406e7`.

G1 und G8 sind nur aus dem Code gelesen, nicht ausgeführt. T2.6 und T7.5 sollen sie belegen.

**Korrekturen:**

- **G1, Touch-Einstellung bei offener App (neu, F7):** Pebble ruft dann `prv_app_resync_cb` bzw. `prv_app_subscribe_cb` in der App-Task auf, also ohne Systemrechte. Darin stecken eine Kern-Sperre und Kern-Variablen (`touch_nav_service.c:113-116`, `:135-147`, `applib/app.c:173-175`, `touch.c:86-111`, `kernel/mutex.c:16-23`). Erwartet ist ein Absturz der App, nicht der Uhr. Neu dazu T2.6 und Gerätetest-Punkt 7.
- **G2, Quick Launch:** Es fehlten Select- und Back-Halten sowie die Kombinationen Back+Up und Up+Down (`watchface.c:57-138`, `:201-216`, `prefs.c:206-249`). Diese blättern im Kreis. Auf der letzten Karte tut die Weiterrichtung nichts (`card_view.c:280-301`, `:325-340`). Quick Launch Unten öffnet ohne Shutter und gespiegelt (`shell.c:23-53`).
- **G3, Einstiege:** „Start vom Telefon“ fehlte (`app_run_state.c:47-67`), er öffnet Today. Wischen oder Tippen am Zifferblatt ist kein Einstieg (`applib/touch_service.c:18-35`).
- **G4, Aufträge aus Syscalls:** `boulder_aufgabe_einreihen` misst auf der App-Task die App-Schlange (8 Plätze), füllt aber die Hauptschlange. Boulders Reserve von 8 Plätzen gilt dann nicht (`takt.c:22-45`, `system_task/service.c:198-204`, `:266-270`).
  - Außerdem widersprach sich S4 („nicht verwerfbar“, aber `false` bei voller Schlange).
  - Neu: `system_task_add_callback` aus der App-Task (`service.c:244-249`), ein Merker je Auftrag und der Stufenwunsch in einer Kernvariable. `false` nur noch bei ungültiger Stufe.
- **G5
11:## 0. Kurzfassung
28:### Befunde zum Bestand (am Code geprüft)
49:## 1. Ziel und Grenzen
51:### 1.1 Inhalt
63:### 1.2 Nicht in Schritt 3
80:## 2. Bildschirme
82:### 2.1 Aufbau
90:### 2.2 Karten (y in px)
100:### 2.3 Details (Select)
110:### 2.4 Navigation und Tasten
120:### 2.5 Texte
137:### 2.6 Daten auffrischen
149:## 3. Datenschnittstelle
151:### 3.1 Regeln
169:### 3.2 Syscalls
179:### 3.3 Strukturen
216:### 3.4 Sperren und Tasks
228:### 3.5 Speicher
242:### 3.6 Was die App von Pebble nutzt
261:### 3.7 Regeln für den Code der App
275:## 4. Ersetzen von Pebbles Health-App (nur emery)
277:### 4.1 Weg (V1)
291:### 4.2 Einstiege
307:### 4.3 Die anderen Boards
313:## 5. Einstellungen auf der Uhr
326:## 6. Was vom Telefon kommt
341:## 7. Pebble-Eingriffe (K1, vollständig)
369:## 8. Arbeitspakete
399:### 8.1 QEMU-Prüfweg und Bildbelege
420:## 9. Abnahmekriterien (Entwurf)
433:### 9.1 Prüfliste
466:## 10. Offene Fragen
468:**F1: Karten oder Liste?**
473:**F2: Nächteliste auf der Uhr?** Die Liste umfasst 37 Wachtage zu je 8 B (300 B in `boulder_v2`; warum 37: 3.3), ist Boulder-Code und kommt mit S5.
482:**F3: Tagesband auf der Uhr?**
487:**F4: Precision auch in der Health-App setzen?**
492:**F5: Werte vom Telefon** (Blutdruck, Gewicht, Tagespuls und Belastung, Koffein, Akku-Restzeit) über den Schlüssel `boulder_werte` in der BlobDB Health (BD 7)?
497:**F6: Kieselstraße vor der ersten Vorabversion?** b26 hat sie, V2 nicht (Schritt 2, 1.2).
502:**F7: Touch-Einstellung bei offener App** (Gegenprüfung, Befunde). Ändert das Telefon „Touch“ oder „Touch Navigation“, während die App offen ist, läuft Pebbles Kerncode in der App-Task ohne Systemrechte. Erwartet ist ein Absturz der App, nicht der Uhr. Dasselbe träfe jede SDK-App, die Touch-Navigation eingeschaltet hat.
509:## 11. Vorgaben zum Bestätigen (sonst so umgesetzt)
531:## 12. Nach Schritt 3: erste Vorabversion
543:### 12.1 Gerätetest, Teil Schritt 3
555:## 13. Belegstand
574:## Gegenprüfung (08.10.2026)
578:### Korrigiert
603:### Geprüft und richtig (ohne Änderung)
615:### Offen nach der Gegenprüfung
~~~~

## 2026-10-08 09:01:40 (toolu_011YDG9ynFUiqdKXLgCwudqw)

Befehl:

~~~~
cd ~/Documents/Claude/boulder-v2 && sed -n 80,120p schritt3-spezifikation.md; sed -n 466,530p schritt3-spezifikation.md
~~~~

Ausgabe:

~~~~
## 2. Bildschirme

### 2.1 Aufbau

- **Fünf Karten**, jede füllt den Schirm (200×228, 64 Farben, U: `fw/board/displays/display_obelix.h:16-17`, `boards/obelix/defconfig:11`).
  - Oben ein Kopfbalken (0–27 px) in der Farbe der Karte, mit dem Titel in GOTHIC 18 fett, weiss. Rechts fünf Punkte zeigen die Karte.
  - Weisser Grund, Zahlen in LECO 26–38, Labels in GOTHIC 14 grau. Vorbild sind b25 und Kieselsport; b25 hatte den Titel in GOTHIC 24 fett und die grosse Zahl in LECO 42 (M: `src/fw/apps/system/boulder_health/karten.c:95`, `:113`).
- **Reihenfolge:** Today → Night → Heart → Activity → Settings.
- **Keine Sätze.** Fehlt ein Wert, steht „–“ in GOTHIC und „-“ in LECO (LECO hat kein „–“, Befunde). Urteile sind farbig: grün für „as usual“, „Well rested“ und „in range“, sonst Bernstein.

### 2.2 Karten (y in px)

| Karte | Inhalt | Daten | Leer, Sonderfall |
|---|---|---|---|
| **Today** | **32–74 Sleep:** „7:12 h“, rechts „Well rested“, „Short night“ oder „Very short night“. Grenzen: ≥ 480 und ≥ 420 min (B: `kiesel/Tagesurteile.kt:86-90`, `Gesundheitsziele.kt:20`).<br>**76–118 Resting:** „54 bpm“, rechts „as usual“, „+3“ oder „−3“. Regel: ±2 um den gerundeten Schnitt von bis zu 7 früheren Nächten aus 8 Tagen, ab 3 Nächten (B: `Tagesurteile.kt:67`, `:125-146`).<br>**120–162 HRV:** „48 ms“, rechts „in range“, „above“ oder „below“. Band ab 3 Werten, Urteil ab 7 (B: `Gesundheitsrechnung.kt:488-522`, `Tagesurteile.kt:160-171`).<br>**164–220 Steps:** „6,230“ und ein Balken bis 10,000 (B: `Gesundheitsziele.kt:14`) | NachtBild, Lage, Nächteliste (F2), Schritte heute | Ohne Nacht steht bei Sleep „–“. Ohne F2 hat HRV kein Urteil, und das Ruhepuls-Urteil kommt aus dem Ring ohne Datum (BD D5). Ist das Tracking aus, steht „Tracking off“ unter dem Titel und überall „–“ |
| **Night** | **32–78:** „7:12 h“ in LECO 38, rechts das Urteil. Bei `fertig = 0` steht statt des Urteils „so far“.<br>**80–100:** „22:14 – 06:00“ (12 oder 24 h nach `clock_is_24h_style`).<br>**106–166:** die Nacht als **ein Block**, 192 px vom Beginn des ersten bis zum Ende des letzten Abschnitts (wie Boulder, B: `ansicht/Gesundheitsbilder.kt:74-75`), aus `NachtBild.abschnitte` (bis 96). Vier Bahnen von oben nach unten: Awake, REM, Light, Deep. Schlaf ohne Phase (`SchlafPhaseSchlaf`, U: `inc/schlafanalyse.h:40-49`) liegt in der Bahn Light, wie in Boulder (B: `ansicht/Gesundheitsbilder.kt:56-58`, `:69`, `:94-97`).<br>**172–222:** vier Spalten Awake, REM, Light, Deep mit der Dauer aus `NachtBild.minuten[0..3]` („0:12“). Schlaf ohne Phase (`minuten[4]`) hat keine Spalte; ist `minuten[1] + minuten[2] + minuten[3] = 0`, entfällt die Zeile (B: `ansicht/NachtAnsicht.kt:247-268`, `Gesundheitsrechnung.kt:225-230`) | NachtBild | Ohne Nacht steht „No night“ in der Mitte. Nickerchen gehören nicht zur Nacht (V5) |
| **Heart** | **32–80:** Puls jetzt in LECO 38 mit „bpm“, rechts „now“ oder „12 min“ (frisch heisst ≤ 15 min, B: `Tagesurteile.kt:71`).<br>**86–150:** Pulsband über 24 h. 48 Halbstunden-Säulen zu 4 px zeigen Minimum bis Maximum in Grau. Darüber eine Linie der Halbstunden-Mittel in der Farbe der Zone (Karvonen 40/60/80 %, B: `Gesundheitsrechnung.kt:466-481`), dazu ein Schatten für die Nacht.<br>**156–222:** drei Spalten: Resting „54“, HRV „48 ms“, SpO2 „96%“ (letzter Wert der 24 h). Bei Stufe 1 steht bei SpO2 „off“ | Puls über `health_service` (1 s nur hier, V3), Minutenverlauf über 30 h, Lage, NachtBild, SpO2 | Ohne Puls in 24 h bleibt das Band leer, die Werte zeigen „–“ |
| **Activity** | **32–80:** Schritte in LECO 38.<br>**84–90:** Balken bis 10,000.<br>**96–118:** „4.8 km · 42 min active“.<br>**126–222:** die Woche als 7 Säulen. Die Wochentage stehen mit zwei Buchstaben darunter, wie in Boulder (B: `ansicht/Schrittwoche.kt`). Heute ist hervorgehoben, das Ziel 10,000 eine durchgezogene Linie | `sys_activity_get_metric` (Schritte über 7 Tage, aktive Sekunden, Strecke), Einheit über `sys_shell_prefs_get_units_distance` | Eine Säule ohne Wert bleibt leer |
| **Settings** (F4) | **32–100:** „Precision“, darunter der Name der Stufe („Balanced“) und fünf Punkte | Lage | Ohne F4 entfällt die Karte |

### 2.3 Details (Select)

| Von | Detail | Inhalt |
|---|---|---|
| Today | **Day** (F3) | Das Tagesband über 24 h bis jetzt, auf einer Seite, wie im Erlebnis-Entwurf vom 06.10.:<br>– **Puls:** Halbstunden in der Farbe der Zone.<br>– **Schlaf und Schritte** teilen sich eine Zeile: Schlaf als Block mit Phasen, Schritte je Stunde als Säulen.<br>– **Sitzungen** als Marken: Walk, Run, Ride, Workout, Nap.<br>– **Zeitachse**, jetzt am rechten Rand.<br>Daten: Minutenverlauf, NachtBild, Sitzungen |
| Night | **Night** | Die Seite scrollt, wie Boulders Nachtansicht (B: `NachtAnsicht.kt:91-250`):<br>– **THROUGH THE NIGHT:** Block mit Phasen, Puls der Nacht aus dem Minutenverlauf und SpO2-Punkte, alle auf einer Zeitachse.<br>– **BODY DURING SLEEP:** Resting, HRV und SpO2, darunter grau „avg“ bzw. „lowest“.<br>– **LAST NIGHTS:** 7 Säulen und der Schnitt. Mit F2 kommen sie aus der Nächteliste, ohne F2 aus Pebbles Tagessummen |
| Heart | **Heart** | Die Seite scrollt:<br>– **ZONES TODAY:** Minuten in Light, Moderate, Vigorous und Peak (B: `res/values/kiesel_gesundheit_strings.xml:67-70`).<br>– **SPO2 24 H:** Punkte.<br>– **HRV:** 7 Nächte mit Band, nur mit F2 |
| Activity | **Sessions** | Eine Liste der Sitzungen von heute (Pebbles Sitzungsliste):<br>– je Zeile Symbol, Art, Beginn, Dauer und Schritte oder kcal;<br>– Arten nach `type` und `manual`: Walk und Run (`manual = 0`, erkannt), Ride (`Open` mit `manual = 0`, die bestätigte Velofahrt, U: `boulder/kontext.c:183-198`), Workout (`manual = 1`, jeder Typ: Kieselsport legt Laufen als Run, Wandern als Walk, alles andere als Open an, U: `boulder/sport/plattform.c:63-75`, `workout_service.c:586`) und Nap. Sleep und Restful* stehen nicht in der Liste.<br>Unten km, aktive Minuten und kcal des Tages (V12) |
| Settings | **Precision** | Die 5 Stufen: Battery Saver, Light, Balanced, Detailed, Precise (U: `boulder/messstufe.c:48-51`). Ein Häkchen markiert die geltende, Select setzt die Stufe und schliesst die Liste |

### 2.4 Navigation und Tasten

| Eingabe | Auf einer Karte | Im Detail |
|---|---|---|
| Oben / Unten | vorige oder nächste Karte. Vom Startmenü aus laufen die Karten im Kreis. Nach Quick Launch mit **Tippen oder Halten auf Oben oder Unten** gibt es keinen Kreis: Die Gegenrichtung auf der ersten Karte führt zurück zum Zifferblatt, auf der letzten Karte tut die Weiterrichtung nichts, ein Start über Unten spiegelt die Richtung. Quick Launch über Select- oder Back-Halten und über die Kombinationen Back+Up und Up+Down blättert im Kreis wie das Startmenü. So macht es Pebble (U: `card_view.c:280-301`, `:325-340`; Geste über `app_launch_get_quick_launch_action`) | scrollen |
| Select | Detail öffnen | in Precision: Stufe setzen |
| Zurück | App schliessen | zurück zur Karte |
| Wischen hoch / runter / links / rechts | wie Unten / Oben / Select / Zurück (U: `fw/applib/ui/recognizer/touch_nav.c:370-388`), automatisch für ID −82 | gleich |
| Start | Startmenü, Quick Launch und ein Start vom Telefon (`APP_LAUNCH_PHONE`) öffnen Today. „Open App“ aus einem Pin (`APP_LAUNCH_TIMELINE_ACTION`) öffnet bei `card_type` 0 Activity und bei 1 Night | – |

### 2.5 Texte
## 10. Offene Fragen

**F1: Karten oder Liste?**
- A: Karten wie Pebbles Health. Jede Karte ist ein Blick, Oben/Unten und Wischen blättern, Select öffnet ein Detail, Pins öffnen eine Karte.
- B: eine scrollende Liste wie Boulders Health-Tab (B: `ansicht/GesundheitAnsicht.kt:133-158`).
- **Empfehlung A.** Auf 228 px liest man eine Karte auf einen Blick, Quick Launch und Pins brauchen einen festen Startpunkt, und Wischen blättert natürlich.

**F2: Nächteliste auf der Uhr?** Die Liste umfasst 37 Wachtage zu je 8 B (300 B in `boulder_v2`; warum 37: 3.3), ist Boulder-Code und kommt mit S5.
- **Mit Liste:** HRV-Band und HRV-Urteil wie in Boulder, ein genaues Ruhepuls-Urteil (8 Tage mit Datum), und „Last nights“ zeigt Boulders Nächte ohne Nickerchen.
- **Ohne Liste:**
  - HRV ohne Urteil;
  - Ruhepuls-Urteil aus dem Ring ohne Datum (BD D5);
  - „Last nights“ aus Pebbles Tagessummen, mit Nickerchen und vom Telefon überschrieben (BD D4).
- **Folge der Liste:** Sie beginnt beim Wechsel mit einer Nacht. Das Ruhepuls-Urteil kommt ab der 4. Nacht, das HRV-Band ab der 3. Nacht, das HRV-Urteil ab der 7.
- **Empfehlung: mit Liste.** Die Uhr funktioniert so ohne Telefon voll (`entscheidungen.md` „Uhr“).

**F3: Tagesband auf der Uhr?**
- A: als Detail der Karte Today, 24 h auf einer Seite (2.3). Es braucht keinen neuen Speicher, nur einmal den Minutenverlauf.
- B: nicht in Schritt 3.
- **Empfehlung A.** `ideen.md` hat heute keinen Eintrag dazu (BP 10.5). Mit A wäre der Punkt erledigt.

**F4: Precision auch in der Health-App setzen?**
- A: ja, als Karte Settings, zusätzlich zu Settings → Health (S4).
- B: nur in Settings → Health. Die App zeigt die Stufe nicht.
- **Empfehlung A.** So liegt „Health“ als Ort der Einstellungen in der Health-App, wie in Boulder. Kosten: ein Syscall und eine Karte.

**F5: Werte vom Telefon** (Blutdruck, Gewicht, Tagespuls und Belastung, Koffein, Akku-Restzeit) über den Schlüssel `boulder_werte` in der BlobDB Health (BD 7)?
- A: schon in Schritt 3. Das braucht eine neue Boulder-Fassung vor dem Gerätetest.
- B: erst in Schritt 4 entscheiden, zusammen mit Water und Stack.
- **Empfehlung B.** Schritt 3 bleibt so ohne Boulder-Änderung, und die Uhr zeigt nur, was sie selbst misst.

**F6: Kieselstraße vor der ersten Vorabversion?** b26 hat sie, V2 nicht (Schritt 2, 1.2).
- A: als eigener kleiner Schritt vor der Vorabversion portieren.
- B: Der Gerätetest läuft ohne sie.
- **Empfehlung A.** Sonst ist die Vorabversion im Alltag ein Rückschritt gegenüber b26, und der Gerätetest dauert mindestens 7 Nächte.

**F7: Touch-Einstellung bei offener App** (Gegenprüfung, Befunde). Ändert das Telefon „Touch“ oder „Touch Navigation“, während die App offen ist, läuft Pebbles Kerncode in der App-Task ohne Systemrechte. Erwartet ist ein Absturz der App, nicht der Uhr. Dasselbe träfe jede SDK-App, die Touch-Navigation eingeschaltet hat.
- A: hinnehmen, in die Release-Notiz schreiben und Pebble melden. Kein Pebble-Code.
- B: eine weitere Pebble-Stelle P8 in `touch_nav_service.c` (Rückruf nur bei Apps mit Systemrechten, oder über einen Syscall).
- **Empfehlung A**, sobald T2.6 den Absturz belegt; bleibt die App heil, entfällt die Frage.

---

## 11. Vorgaben zum Bestätigen (sonst so umgesetzt)

- **V1 Weg und Ersetzen:** eingebaut ohne Systemrechte (BP 6, Weg A), gleiche ID, UUID und `md_fn`, dazu P7 (BP 5.6 a).
  - Nicht gewählt: die Ressourcen-App wie Golf (Weg B). Sie bräuchte einen SDK-Export, baut für alle Boards, und die Registry filtert sie nicht (BP 6.9).
  - Nicht gewählt: ein eigener Registry-Eintrag. Damit ginge der Übergang der Shell verloren (BP 5.6 c).
  - Denkbar, aber nicht geprüft: Pebbles Ordner aus `boulder/quellen.cmake` mit `set_source_files_properties(… DIRECTORY …)` ausschliessen. Das wären 0 Pebble-Zeilen, aber versteckt.
- **V2 Schmale Schnittstelle:** S1–S4, mit F2 dazu S5. Nachtbuch, HRV-Fenster, `KsStand` und Werte vom Telefon kommen nicht in Schritt 3 (BD 5 hatte 8 Syscalls).
- **V3 Puls im Sekundentakt** nur auf der Karte Heart und höchstens 2 min, sonst kein Abo. So war es in b25 (M: `src/fw/apps/system/boulder_health/health.c:12-25`, `:140-156`). Pebbles App hält 1 s, solange sie offen ist (U: `health.c:91`).
- **V4 Ziele fest wie in Boulder:** Schlaf 480 min, 10 000 Schritte, 30 aktive Minuten (B: `Gesundheitsziele.kt:14-26`). Eine Einstellung dafür gibt es nicht.
- **V5 Nickerchen** gehören nicht zur Nacht, sie stehen als „Nap“ in den Sitzungen. Insights und Pins zählen weiter Pebbles Summe samt Nickerchen (BP 5.5). Dafür gibt es keinen Pebble-Code. Typische Werte und 30-Tage-Schnitte vom Telefon nutzt die App nicht.
- **V6 SpO2 nach einem Neustart:** Der Ring bleibt leer, die App zeigt „–“. Der Kommentar `inc/spo2.h:22` wird richtiggestellt (BD D1).
- **V7 Einheiten und Uhrzeit** kommen aus Pebbles Prefs (km oder mi, 12 oder 24 h).
- **V8 Kein Onboarding, kein Modal, keine Erklärsätze.** Das ersetzt U: `health.c:73-82` und `:103-119`. Ist das Tracking aus, steht „Tracking off“; fehlt die Zeit, steht „–“. `health_app_opened_version` wird nicht geschrieben, nur Pebbles App liest ihn (`grep`).
- **V9 Titel „Heart“** statt „Heart & Body“, solange Körperwerte fehlen (F5).
- **V10 Aussehen** wie b25 und Kieselsport. Die Phasenfarben folgen Boulder: wach warm, REM hellblau, leicht, tief dunkelblau (M: `boulder_health/karten.c`, `prv_phasenfarbe`). Den Kontrast (mindestens 3:1) prüft der Render-Test.
- **V11 Kachel im Startmenü:** die allgemeine, ohne Werte.
- **V12 kcal des Tages** sind Pebbles Summe aus den Schritten. Die Pulskalorien eines Trainings stehen nur bei seiner Sitzung (BD D7; Schritt 2, F5 (b) = A).
- **V13 Autofahrten** sind nicht sichtbar, es gibt keine Daten (BD D6).
- **V14 Pebbles 52 Health-Tests** (8 Dateien) bleiben und laufen weiter gegen Pebbles Ordner.

---
~~~~

## 2026-10-08 10:05:14 (toolu_01PQQARXGu6MDRuKnxZrp3ur)

Befehl:

~~~~
ls -l --time-style=+%H:%M ~/Documents/Claude/boulder-v2/schritt3-spezifikation.md; date +%H:%M; grep -c -i 'koffein' ~/Documents/Claude/boulder-v2/schritt3-spezifikation.md
~~~~

Ausgabe:

~~~~
-rw-r--r-- 1 dysseus dysseus 76193 11:00 /home/dysseus/Documents/Claude/boulder-v2/schritt3-spezifikation.md
12:05
3
~~~~

## 2026-10-08 11:07:06 (toolu_01T3hZDQSDwgfjBe4D46VhCB)

Befehl:

~~~~
cat >> ~/Documents/Claude/boulder-v2/entscheidungen.md <<'EOF'
- **F8–F10 (08.10., wie empfohlen):**
  - F8 A: Pulsband und Zonen von heute führt die Variante laufend mit, die App liest keinen Minutenverlauf.
  - F9 A: eigene kleine Wochen-Liste der Schritte auf der Uhr, nicht Pebbles Verlauf.
  - F10 A: Die Ziele kommen über die BlobDB Health (`boulder_ziele`, 8 B), ohne Pebble-Code. Ohne Telefon gilt der zuletzt empfangene Wert, sonst die Vorgabe.
EOF
cd ~/Documents/Claude/boulder-v2 && awk '/^## 8\. Arbeitspakete/,/^### 8\.1/' schritt3-spezifikation.md | cut -c1-260 | head -30
~~~~

Ausgabe:

~~~~
## 8. Arbeitspakete

Jedes Paket läuft in einem eigenen Worktree von `v2` und wird nach jedem fertigen Punkt committet (`/tmp` ist flüchtig). Fertig ist ein Paket, wenn seine Tests grün sind, die Baseline gleich bleibt (13) und obelix und qemu_emery bauen. AP0 und AP9 bauen alle s

| AP | Inhalt | Tests, Prüfweg | braucht | parallel |
|---|---|---|---|---|
| **AP0 Gerüst** | Kconfig, P7, Ordner `boulder_health/` mit einer leeren App ohne Systemrechte, K1-Wächter (P7, Ordner), Test-CMake, Schalter `CONFIG_BOULDER_HEALTH_ZEITEN` | K1-Gegenproben (zweite Zeile in P7, Hunk ohne Schalter, Zeile in Pebbles Health-Ordn
| **AP1 Probe ohne Systemrechte** | Die leere App benutzt im Prüfbau jeden Baustein aus 3.6 und jeden Helfer aus 3.7.5 einmal. Stapel: frisch booten, Probe, dann liest ein Prüfbefehl `boulder stapel` den Tiefstand (U: `syscall_internal.c:372-382`). `pruefe_ohn
| **AP2 Schnittstelle und Ziele** | S1–S4, Lage Fassung 2 im Minutenteil, Aufträge „Bild laden“, „Messstufe“, „Ziele“ nach Regel 3.1.6, `boulder/ziele.c` (3.8.4), Namen der Stufen in `inc/messstufe.h`, Kommentar `inc/spo2.h:22` richtigstellen. Prüfbefehl `boul
| **AP3 Nächte** (F2) | 3.8.1 mit S5 | 10 Referenznächte in Folge mit den echten `nacht.c` und `speicher.c`, Folgenacht, Neustart, Zeitzone, Uhr zurück, 40 Nächte (Liste läuft über), Saat beim Wechsel; Regeln gegen B: `Gesundheitsrechnung.kt:357-376`; Schreibl
| **AP4 Herz und Woche** (F8, F9) | 3.8.2 und 3.8.3 mit S6, S7, Auftrag „Herz nachrechnen“, Ablage „woche“ | T4.4, T4.5. Minutenteil: kein Heap, keine Datei, O(1) (S1-T7.1). Nachrechnen: ein Durchlauf, ohne Heap. Schreiblast | AP2; F8, F9 | AP3, AP6 |
| **AP5 Rechnungen** | Reine Funktionen in `boulder_health/rechnung.c`: Schlafurteil mit Ziel, Ruhepuls-Urteil, HRV-Band und -Urteil (Woche wie `bandwoche`), Zonenfarben, Formate | `test_bh_rechnung` mit einem Orakel aus Boulders Kotlin (`Tagesurteile.kt`, `Ge
| **AP6 Bildschirme und Laden** | Karten, Details, Navigation (2.4), Startgründe, Puls-Takt (V3), Precision (F4), Laden und Auffrischen (2.6.2), Zeitstempel (2.6.3), Texttabelle (2.5) | `test_bh_karten` (jede Karte und jedes Detail voll, leer, Tracking aus, `f
| **AP7 Einstiege** | Registry-Generator und Startmenü für obelix und asterix, Quick Launch, Insights-Pin mit `card_type`, Timeline-Aktion | T3.1–T3.5 | AP0 | AP1, AP2 |
| **AP8 QEMU, Ladezeit, Ressourcen** | Prüfbau qemu_emery (8.1). L1, L2, L4, L6 messen. Tiefstand des App-Heaps, Kernel-Heap vor und nach 100-mal Öffnen und Schliessen, Puls-Abos nach dem Schliessen, längster Callback, Neustarts, Fehlerprobe am Endstand, `.bss
| **AP9 Integration** | Zusammenführen, alle Boards, Kriterien von Schritt 1 und 2, Mutationen (eine bis zwei je Kriterium), Bericht `schritt3-abnahme.md` mit Belegen in `belege-schritt3/` | alle | alle | – |

**Wellen:** 1. AP0. 2. AP1, AP5, AP7. 3. AP2. 4. AP3, AP4, AP6. 5. AP8, AP9. Kritischer Pfad: AP0 → AP1 → AP2 → AP4 → AP6 → AP8 → AP9.

**Antworten nötig:** F10 vor AP2. F8 und F9 vor AP4 und AP6.

### 8.1 QEMU-Prüfweg und Bildbelege
~~~~
