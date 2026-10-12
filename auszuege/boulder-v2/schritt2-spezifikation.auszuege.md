# Auszüge: `boulder-v2/schritt2-spezifikation.md`

KEINE vollständige Datei. Die Datei selbst wurde von Workflow-Agenten geschrieben, deren Schreibvorgänge nicht im Transkript stehen. Hier stehen nur Befehle der alten Sitzung (Sitzung `session_011udJ51Jg5Y9cBcFvGfK26z`), die Teile davon gelesen haben, mit ihrer Ausgabe, wörtlich. Ausgaben können auch andere Dateien betreffen.

## 2026-10-07 09:13:01 (toolu_01XEuMrZkB365EoRbXGd4PDk)

Befehl:

~~~~
cd ~/Documents/Claude/boulder-v2 && wc -l schritt2-spezifikation.md && grep -n -E '^\*\*F[0-9]|^- \*\*F[0-9]|^\| *F[0-9]|^#+ .*(Frage|Vorgabe|Kriter)|^\*\*V[0-9]+|^- \*\*V[0-9]+|^\| *V[0-9]+' schritt2-spezifikation.md | head -40
~~~~

Ausgabe:

~~~~
366 schritt2-spezifikation.md
258:## 7. Offene Fragen
260:**F1: Kieselsport als `.pbw` oder in der Firmware?**
265:**F2: Wer hält im Training das 1-Hz-Puls-Abo?**
271:**F3: Verwerfen.** Pebbles Stopp speichert ab 60 s immer.
277:**F4: Training und erkannte Fahrt bei Boulder auseinanderhalten.**
282:**F5: Kalorien ohne Puls und Tagessumme.**
292:**F6: Startmenü-Komfort aus b25.**
297:### Vorgaben zum Bestätigen (sonst so umgesetzt)
299:- **V1 Velo heißt Art 1 und 4.** Kraft, Yoga und Schwimmen laufen als Open und zählen Schritte wie Pebble. Der Entscheid nennt nur Velo.
300:- **V2 Maximalpuls:** Die Uhr übernimmt Boulders Prüfung (ein eingestellter Wert gilt nur, wenn er > Ruhepuls + 20 und ≤ Tanaka + 25 ist) für Zonen und kcal. Betrifft nur Werte über Tanaka + 25 (Befund 7).
301:- **V3 Ruhepuls** für die kcal: Median der letzten 7 Nächte wie in Boulder, als Ring von 7 Byte in `boulder_v2`, Ersatz 60.
302:- **V4 Pebbles Meldung bleibt.** Kieselsport ruft den Stopp 30 s nach „Speichern“, damit die Meldung den Endschirm nicht verdeckt, wie in b25. In der Pause ändern sich Dauer, Schritte und kcal nicht (U: `:192`, `:249`). Startet in diesen 30 s ein neues Training, stoppt Kieselsport sofort. Ein Neustart in diesen 30 s kostet Pebbles Sitzung, die Zusammenfassung liegt dann schon im Persist. Beim Auto-Ende stoppt Kieselsport sofort. Anders als b25: Die Meldung zeigt Pebbles 3 Zonen und Pebbles Mittel aus den Rohwerten (U: `:575-576`, `:606-607`). b25 ließ die Zonen bewusst weg (M: `plattform.c:286-292`). Pebbles Sitzung kappt nicht auf uint16 (U: `:569-571`, b25: M: `plattform.c:276-280`), und beim Schwimmen trägt sie die Meter aus den Schritten statt aus den Bahnen. Boulder liest diese Sitzung nicht (F4 = A), es betrifft nur die Uhr.
303:- **V5 HRM-Szene:** Yoga und Schwimmen setzt Kieselsport nach dem Start auf Default wie in D: (`plattform.c:118-143`). Pebble setzt bei jedem Open HighIntensity (U: `:148-161`).
304:- **V6 Init ohne Pebble-Haken** (2.5, Regel 5). Liegengebliebenes nach einem Boot am Ladekabel geht erst nach dem Abstecken oder beim App-Start hinaus.
305:- **V7 Bekannte Lücken bleiben:** Timeline-Pin ohne Erzeuger, Kurve endet nach 3 h 20 min, Training ohne Pause endet nie, Nachsenden nur vom Zifferblatt, kein Training über einen Neustart. Das **Archiv** (D: `training.c:193-205`) wird nie gelesen und entfällt. Seine Schlüssel 10, 11 und 100–119 bleiben reserviert.
306:- **V8 Sync-Anstoß:** Statt `health_sync_schicken` (in M: ein Boulder-Zusatz im Pebble-Code) ruft Kieselsport 90 s nach dem Stopp `dls_send_all_sessions`, nur bei Platz.
307:- **V9 Alte Store-App:** Die Firmware blendet sie nicht mehr aus (M: `app_install_manager.c:259-273` entfällt). AP6 prüft mit einem b25-Abbild, ob sie doppelt erscheint. Wenn ja, kommt in die Release-Notiz: einmal löschen. Nur falls dir das nicht reicht, wird es eine weitere Pebble-Stelle.
308:- **V10 SpO2 bei Aktivität** (`bloodOxygenActivity`, Vorgabe aus): Die Pause dafür trifft nur Pebbles ungenutztes Abo (U: `workout_service.c:358-373`). Ist die Option an, kann die Messung im Training scheitern. Nicht behoben.
309:- **V11 Einbrüche:** Boulder wirft vor der Rechnung kurze Einbrüche weg (B: `Trainingsanalyse.kt:201-246`). Die Uhr übernimmt das nicht: Es bräuchte 45 s Vorschau, also einen Puffer je Sekunde. Die Abweichung geht in die ±5 % von K5. Fällt T5.3 deshalb durch, kommt das Entstören als Puffer mit 45 Werten in `kalorien.c`.
310:- **V12 Nur obelix und qemu_emery:** Boulders Locker führt Kieselsport für jede Uhr außer aplite (B: `libpebble3/…/locker/SystemApps.kt:25`). Auf den anderen Boards stünde Kieselsport dort ohne App auf der Uhr. Das bleibt bis zum App-Schritt so, weil du nur Uhren mit Variante nutzt.
~~~~

## 2026-10-07 09:13:05 (toolu_01BF7syhRtyp9yKHMvkipmMX)

Befehl:

~~~~
cd ~/Documents/Claude/boulder-v2 && sed -n 258,296p schritt2-spezifikation.md
~~~~

Ausgabe:

~~~~
## 7. Offene Fragen

**F1: Kieselsport als `.pbw` oder in der Firmware?**
- A: `.pbw` mit eigenem SDK (Syscalls, Revision 110). Update über den Store, dafür viel Pebble-Code, und ein Worker darf im Hintergrund nicht brummen und nicht senden.
- B: System-App in der Firmware wie b25, gleiche UUID, Dienst auf KernelBG.
- **Empfehlung B** (2.1).

**F2: Wer hält im Training das 1-Hz-Puls-Abo?**
- A: Kieselsport wie b25 (KernelBG, 1 s, Sparmodus 5 s, Pause 10 s, minütlich erneuert, Ablauf 5 min). Pebbles Dienst bekommt dieselben Werte über den Rundruf. Kein Pebble-Code.
- B: Pebbles `frontend_opened/closed` aus der App. Ein Training endet dann 60 min nach dem Schließen der App (Verlassen-Regel, U: `:30-34`), die Meldung zeigt auf die versteckte Workout-App (U: `apps/system/workout/utils.c:81`).
- C: Pebbles Abo auf KernelBG verlegen (FIXME U: `:375`). Kostet ≈ 15 weitere Zeilen in P1, Sparmodus und 10-s-Pause bräuchten noch mehr.
- **Empfehlung A.** Pebble wertet den Puls weiter aus, nur das Abo bleibt bei Kieselsport. So läuft ein Training auch bei geschlossener App stundenlang.

**F3: Verwerfen.** Pebbles Stopp speichert ab 60 s immer.
- A: In P1 verhindert ein Merker aus Kieselsport das Speichern, ≈ 3 Zeilen. Keine Sitzung, keine Meldung.
- B: Verwerfen entfällt. Jedes Training ab 60 s bleibt, löschen lässt es sich nur in Boulder oder Health Connect.
- C: Die Sitzung nach dem Stopp aus Pebbles Liste löschen (Pebble-Interna). Die Meldung kommt trotzdem, und der DLS-Satz kann schon unterwegs sein.
- **Empfehlung A.**

**F4: Training und erkannte Fahrt bei Boulder auseinanderhalten.**
- A: DLS 84 wieder in Fassung 7 mit `manual`, wie b25 (P4, ≈ 6 Zeilen). Boulder 0.52 liest das schon und überspringt Trainings. Die Satzgröße ist wieder 27 B wie in M:.
- B: Fassung 3 bleibt, Boulder gleicht ab. Das braucht eine neue Boulder-Fassung vor dem Gerätetest. Bis dahin steht jedes Training in Health Connect doppelt („Workout“/„Run“ und Kieselsport).
- **Empfehlung A.** Mit B würde 0.52 zum Gerätetest nach Schritt 3 jedes Training doppelt eintragen.

**F5: Kalorien ohne Puls und Tagessumme.**
- (a) Sekunden ohne gültigen Puls:
  - A: 0, wie Boulder.
  - B: Pebbles Schrittformel. Laufen und Wandern bekommen etwas, Velo bleibt 0.
  - C: ein MET-Wert je Art.
- (b) Tagessumme der Uhr (ActiveKCalories):
  - A: unverändert. Pulskalorien stehen nur in Sitzung, Endschirm und Zusammenfassung. Velo-Trainings fehlen dann in der Tages-kcal der Uhr.
  - B: Pulskalorien auch in der Tagessumme. Das braucht eine weitere Stelle in `activity.c`.
- **Empfehlung (a) B, (b) A.** Ein Lauf ohne Puls bekommt so wenigstens Pebbles Wert, und es kommt kein Eingriff in `activity.c` dazu.

**F6: Startmenü-Komfort aus b25.**
- A: Wie b25: Kieselsport rückt im Training nach oben, und die Kachel zeigt „‖ h:mm:ss“. Kostet P5 (2 Zeilen) und P6 (≈ 8 Zeilen).
- B: Kein Pebble-Code. Die normale App-Kachel nutzt `app_glance_db` und funktioniert für jede UUID (U: `app_glance_service.c:268-271`), mit laufender Dauer per Vorlage (`time_since`, U: `fw/applib/template_string.c:68-69`). Der Dienst schreibt sie von KernelBG mit `app_glance_db_insert_glance` (U: `include/pbl/services/blob_db/app_glance_db.h:27`). Die Vorlage zählt Wandzeit, deshalb schreibt er die Kachel bei Start, Pause, Weiter und Ende neu (je ein Flash-Schreiben). Kieselsport rückt nicht nach oben (die Liste `app_menu_data_source.c:43-50` kennt es nicht), öffnet bei laufendem Training aber direkt den Laufschirm (A: `kieselsport.c:134`).
- **Empfehlung B.** Quick Launch lässt sich auf Kieselsport legen.
~~~~

## 2026-10-07 10:02:07 (toolu_01SABpFQC8t4vSYgM8xkNNCn)

Befehl:

~~~~
cd ~/Documents/Claude/boulder-v2 && grep -n -E '^#+ ' schritt2-spezifikation.md | head -40; grep -n -E '^\| \*\*AP[0-9]' schritt2-spezifikation.md | cut -c1-220
~~~~

Ausgabe:

~~~~
1:# Boulder V2, Schritt 2: Spezifikation „Kieselsport auf dem Workout-Dienst“
11:## 0. Kurzfassung
22:### Befunde zum Bestand (am Code geprüft)
40:## 1. Ziel und Grenzen
42:### 1.1 Inhalt
55:### 1.2 Nicht in Schritt 2
67:## 2. Architektur
69:### 2.1 `.pbw` oder Firmware (F1)
81:### 2.2 Wer macht was
95:### 2.3 Pebble-Eingriffe (K1, vollständig)
110:### 2.4 Ordner und Dateien
125:### 2.5 Regeln zur Laufzeit
135:### 2.6 Schnittstelle zur Variante (nur Boulder-Dateien)
143:## 3. Kalorien
163:## 4. Datenflüsse zu Boulder 0.52
181:## 5. Arbeitspakete
218:## 6. Abnahmekriterien
233:### 6.1 Prüfliste (Gegenprüfung: die Tests waren nur genannt)
258:## 7. Offene Fragen
297:### Vorgaben zum Bestätigen (sonst so umgesetzt)
314:## 8. Belegstand
331:## 9. Gegenprüfung (07.10.2026)
187:| **AP0 Gerüst** | Kconfig, `quellen.cmake`, Ordner, P3, leere App, K1-Wächter (P1–P4 mit Zeilengrenzen, `EIGENE_ORDNER` um App- und Testordner erweitert), Vertrag `workout_service.c`, Test-CMake | K1-Gegenproben (Ze
188:| **AP1 Pulskalorien und Verwerfen** | `kalorien.c`, P1, Profil-Schnappschuss, Ersatz ohne Puls | T5.1 Fälle aus B: `TrainingsanalyseTest.kt:74-153` ≤ 1 %; T5.2 Pause, Lücke, Off-Wrist; T3.1 Verwerfen: keine Sitzung,
189:| **AP2 Dienst** | Port D: → `boulder/sport/`, `plattform.c` neu auf dem Workout-Dienst, Abo (F2), Regeln 2.5, Maxpuls (V2), Persist-Schlüssel wie D: | 47 M:-Diensttests (`test_kieselsport` 42 + `test_kieselsport_spe
190:| **AP3 App** | a: Telefon, Meldungen, Einstellungen, Sendeschirm (mit Attrappe des Dienstes). b: Laufschirm, Menü, Kachel (F6), verzögerter Stopp (V4) | a: 29 M:-Tests (`test_kieselsport_telefon` 19, `_meldungen` 7,
191:| **AP4 Variante** | D5′ Velo-Training (Schritte weder in Summe noch in Minute, Rate 0), Fahrt beim Trainingsstart (heute endet sie erst mit ihrer letzten fahrtartigen Minute, U: `kontext.c:238-245`), Kontext-Ring be
192:| **AP5 Sichtbarkeit, DLS 84** | P2; P4 und `vertraege.c` (F4), beide nur mit `CONFIG_BOULDER_KIESELSPORT` | T6.1 Startmenü und Quick Launch ohne Workout, auf asterix mit Workout; `SitzungsParserTest` mit V2-Bytes; S
193:| **AP6 Wechsel b25 → V2** | Persist-Abbild aus einem M:-Lauf (Einstellungen, Wartendes, Stopp, Kurve) in V2 lesen und genau einmal senden; alte Store-App (V9) | T6.2, T6.3 | AP2, AP3a | AP3b |
194:| **AP7 QEMU und Ressourcen** | Prüfbau mit Shell-Befehlen `kieselsport start/pause/weiter/speichern/verwerfen` und eingespeistem Puls (qemu_emery hat `HRM_STUB`, keinen Kompass); 2-h-Training, Folgen, Heap, Watchdog
195:| **AP8 Integration** | Zusammenführen, alle Boards, Kriterien von Schritt 1, Bericht `schritt2-abnahme.md` mit Belegstand | alle | alle; Referenztrainings | – |
~~~~

## 2026-10-07 10:02:26 (toolu_019zqjT6912QFAfDRR7TVNk8)

Befehl:

~~~~
cd ~/Documents/Claude/boulder-v2 && python3 - <<'EOF'
p='entscheidungen.md'; s=open(p).read()
alt1='- **F2:** Der Nutzer will mehr Erklärung, offen.\n'
alt2='- **F3:** Antwort „verwerfen“ ist unklar zwischen A (Verwerfen möglich, Merker in P1) und B (Verwerfen entfällt), Rückfrage gestellt.\n'
assert alt1 in s and alt2 in s
s=s.replace(alt1,'- **F2: A.** Kieselsport hält das 1-Hz-Puls-Abo wie b25: KernelBG, 1 s, Sparmodus 5 s, Pause 10 s, minütlich erneuert, Ablauf 5 min. Pebble wertet über den Rundruf mit aus.\n')
s=s.replace(alt2,'- **F3: A.** Ein kleiner Merker in Pebbles Dienst (P1, etwa 3 Zeilen) verhindert das Speichern eines verworfenen Trainings: keine Sitzung, keine Meldung.\n')
open(p,'w').write(s)
EOF
cat >> schritt2-spezifikation.md <<'EOF'

## 10. Entscheide des Nutzers (07.10.2026), massgebend vor Abschnitt 7
- **F1:** B, Firmware-System-App. Kieselsport ersetzt weiterhin Pebbles Sport-App.
- **F2:** A, Kieselsport hält das Puls-Abo.
- **F3:** A, ein Merker in P1, verworfen wird nicht gespeichert.
- **F4:** A, DLS 84 in Fassung 7 mit `manual`.
- **F5:** (a) B, (b) A.
- **F6:** B, kein Pebble-Code für das Startmenü.
- **V1 GEÄNDERT:**
  - Schritte zählen im Training nur bei Sportarten mit Schritten: Laufen, Wandern, Gehen.
  - Alle anderen Kieselsport-Arten (Velo, MTB, Kraft, Yoga, Schwimmen) zählen 0 Schritte, weder in der Summe noch in der Minute, Rate 0.
  - Damit gilt D5′ in AP4 für alle diese Arten, nicht nur für Velo/MTB. Sie laufen als „Open“.
- **V7, V9:** bestätigt.
- **Übrige V:** gelten wie oben.
- **Wecker:** Die Lücke „Neustart im Weckfenster“ wird vor AP0 geschlossen. Ein unterbrochener Zwischenstand liegt auf `origin/v2-wecker-zwischenstand` (af95791af).
EOF
tail -3 schritt2-spezifikation.md
~~~~

Ausgabe:

~~~~
- **V7, V9:** bestätigt.
- **Übrige V:** gelten wie oben.
- **Wecker:** Die Lücke „Neustart im Weckfenster“ wird vor AP0 geschlossen. Ein unterbrochener Zwischenstand liegt auf `origin/v2-wecker-zwischenstand` (af95791af).
~~~~
