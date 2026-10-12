# Auszüge: `boulder-v2/fakten-innen.md`

KEINE vollständige Datei. Die Datei selbst wurde von Workflow-Agenten geschrieben, deren Schreibvorgänge nicht im Transkript stehen. Hier stehen nur Befehle der alten Sitzung (Sitzung `session_011udJ51Jg5Y9cBcFvGfK26z`), die Teile davon gelesen haben, mit ihrer Ausgabe, wörtlich. Ausgaben können auch andere Dateien betreffen.

## 2026-10-05 13:51:53 (toolu_017nkAEUuPC4Hz5ZRTNfjwQc)

Befehl:

~~~~
cd ~/Documents/Claude/boulder-v2 && grep -n -i -E "activity_algorithm\.h|schnittstelle|einhaeng|einhäng|E[0-9]\b|algorithm_" fakten-innen.md | head -60
~~~~

Ausgabe:

~~~~
11:### 1.1 Schichten und Algorithmus-Schnittstelle
12:- **Schnittstelle:** Es gibt eine austauschbare Algorithmus-Schnittstelle, U: include/pbl/services/activity/activity_algorithm.h:209-377. Sie hat 21 Funktionen:
20:- **Der Glue tut mehr als rechnen:** U: fw/services/activity/kraepelin/activity_algorithm_kraepelin.c hat 1424 Zeilen. Er führt außerdem die Minutendatei und den Minuten-DLS (Tag 81), die Minutenhistorie für Apps und das Nullen der Schritte im Schlaf. Wer ihn komplett ersetzt, muss das alles nachbauen.
30:  2. `activity_algorithm_minute_handler`
33:- **Ausnahmen von „alles auf KernelBG“** (die Doku in activity_algorithm.h:16 behauptet das):
34:  - `activity_algorithm_enable_activity_tracking` kommt aus dem Task des Workout-Starters, bei der Workout-App ist das App (U: fw/services/activity/workout_service.c:485).
35:  - `activity_algorithm_early_deinit` läuft im Task des Ladekabel-/Runlevel-Ereignisses und hält dabei die Activity-Sperre (activity.c:1319-1343).
39:- **Minutensatz:** `prv_fill_minute_record` (activity_algorithm_kraepelin.c:883-926) füllt:
45:- **Ringpuffer:** Die Minuten gehen in einen Ringpuffer mit 35 Plätzen (15+19+1; activity_algorithm_kraepelin.c:38-39, 625-664). Geschrieben werden erst 15 „sichere“ Minuten. Im Schlaf hält Kraepelin bis zu 19 unsichere Minuten zurück (KALG_MAX_UNCERTAIN_SLEEP_M, kraepelin_algorithm.h:63).
47:- **Minutendatei:** Settings-Datei „activity_sleep“, 0x20000 B = 128 KiB (activity_algorithm.h:194).
48:  - Schlüssel = utc/900. Ein Satz = 15 Minuten v6 zu je 7 B (Schritte, Lage, VMC, Licht, Flags, HR). Kein SpO2, keine HRV, keine Kalorien (activity_algorithm.h:47-92, 184).
50:  - Ist die Datei voll, wird sie auf die Hälfte verdichtet (activity_algorithm_kraepelin.c:440-450). Real bleiben also 5–11 Tage.
51:- **Minuten-DLS:** Tag 81, v14, 279 B je 15 Minuten, ungepuffert (activity_algorithm.h:109-181; activity_algorithm_kraepelin.c:468-488).
52:- **Schritte im Schlaf:** Schritte in Minuten, die Kraepelin für „sicher geschlafen“ hält, werden nachträglich von der Tagessumme abgezogen und in Datei und DLS genullt (activity_algorithm_kraepelin.c:419-423, 504-515). Das hängt an Kraepelins Schlafzustand.
53:- **Historie für Apps:** `get_minute_history` geht mit `settings_file_each` über die ganze Datei, hält dabei die Kraepelin-Sperre und hängt die RAM-Minuten an (activity_algorithm_kraepelin.c:1211-1274, 1165-1208).
57:- **Speicher:** KAlgState hat 1520 B (ELF; Upstream ohne unser movement_state ≈ 1492 B), AlgState 1344 B. Beides liegt im Kernel-Heap und wird beim Start des Trackings angelegt (activity_algorithm_kraepelin.c:777-818).
62:  - Sitzungen werden nur über `prv_create_activity_session_cb` gemeldet (activity_algorithm_kraepelin.c:133-175).
67:- **Eingaben je Minute:** Nur VMC, Lage und „sicher nicht getragen“. Letzteres heißt: am Ladekabel oder ein HRM-Off-Wrist, das jünger als 15 min ist (activity_algorithm_kraepelin.c:947-965; activity_private.h:136). **Puls und HRV fließen nicht in den Schlaf ein.**
80:- **Nickerchen:** Schlaf von höchstens 3 h, der ganz zwischen 12 und 21 Uhr liegt, wird zum Nap (activity_algorithm_kraepelin.c:695-774; kraepelin/activity_algorithm_kraepelin.h:19-31).
104:- **ActivitySleepState wird nur indirekt abgeleitet** (activity_sessions.c:395-415). „Schlafend“ gilt nur, wenn das letzte Sitzungsende höchstens 1 min vom Ergebnis von `activity_algorithm_get_last_sleep_utc()` abweicht (Kraepelin-Minute − 4 min). Sonst gilt „wach“. Wer diesen Zustand lesen:
187:- **Kanäle:** 8, belegt von KernelMain, KernelBG, NewTimers und PULSE (Kconfig:6-32; fw/main.c:149, 331; service.c:105; pulse2.c:255).
288:  - `boulder_schlaf_ersetzt_kraepelin` in activity_algorithm_kraepelin.c:169
294:  - Gehfilter unter 40 Schritte/min (activity_algorithm_kraepelin.c:190)
296:  - Laufende Bewegung wird beim Pausieren abgeschlossen (activity_algorithm_kraepelin.c:1084-1119)
306:  - Ablauf je Takt: `kernel_malloc` 19.924 B → `activity_algorithm_get_minute_history` über bis zu 960 Minuten (voller Datei-Scan unter der Kraepelin-Sperre) → Schlafanalyse → Sitzungen ersetzen → Bild und Zustand in die Datei „boulder_nacht“ (2 KiB) → bei fertiger Nacht 2× `dls_create`, das bis zu 4 s blockieren kann → 90 s später Health-Sync.
342:- **Lesen der Minutenhistorie setzt die laufende Minute zurück:** `get_minute_history` → `prv_fill_minute_record` → `kalg_minute_stats` setzt VMC- und Lage-Zähler der laufenden Minute auf null (activity_algorithm_kraepelin.c:1195-1203; kraepelin_algorithm.c:1368-1370). Jede App, die mitten in der Minute liest, drückt die VMC dieser Minute. Das ist im Code belegt, aber nicht gemessen; auch in unserem main.
347:  - Minute: Kraepelin-Sperre → Activity-Sperre (activity_algorithm_kraepelin.c:969-977 → 173 → activity_sessions.c:159)
359:## 6. Saubere Einhängepunkte
371:- **E1, der beste Datenpunkt:** activity.c:939-947. Nach `activity_algorithm_minute_handler` liegt der komplette Minutensatz v14 im RAM (`minute_record`): Schritte, VMC, Lage, Licht, plugged_in, Kalorien, Strecke, HR-Median mit Gewicht und Zone, SpO2. Er gilt für utc−60 (activity_algorithm_kraepelin.c:941), und die Schritte sind dort noch nicht schlafgenullt.
373:- **E2:** `kalg_activities_update` (activity_algorithm_kraepelin.c:962-965). Dieselbe Minute, wie sie die Detektoren sehen, inklusive `not_worn`. Ein zweiter Detektor ließe sich hier parallel füttern.
374:- **E3, Sitzungen filtern oder übersetzen:** `prv_create_activity_session_cb` (activity_algorithm_kraepelin.c:133-175) ist der einzige Weg, auf dem Kraepelin-Sitzungen entstehen. Der Fork nutzt ihn schon.
375:- **E4, Nachbearbeitung der Schlafsitzungen:** `activity_algorithm_post_process_sleep_sessions` (Aufruf activity_sessions.c:643-649, Glue 695-774). Hier lassen sich Sitzungen umbenennen oder zusammenlegen, wenn sie fest sind.
376:- **E5, Schlafzustand direkt setzen statt ableiten:** in `prv_update_sleep_metrics` (activity_sessions.c:361-445).
379:- **E6, Smart Alarm:** Eine Funktion, `prv_should_smart_alarm_trigger` (alarm.c:191-207). Entweder ersetzen, etwa durch Phase, Puls oder Bewegungstrend, oder ohne Eingriff nur über korrekte Werte für ActivityMetricSleepState und LastVMC steuern.
380:- **E7:** Schlafsumme und Restful in `prv_compute_sleep_stats` (activity_sessions.c:302-349). Dort hängt heute schon unser Haken `boulder_schlaf_minuten`.
383:- **E8:** activity_algorithm.h komplett neu implementieren und die Quelle in der CMakeLists tauschen. Das sieht Upstream so vor, aber man müsste Minutendatei, DLS 81, Historie und Schritt-Nullung nachbauen. Kraepelin für Schritte zu behalten und nur den Schlaf über E1/E2/E5 zu ersetzen ist deutlich kleiner.
~~~~

## 2026-10-05 13:51:55 (toolu_01AG1UXpq812Bb1TvKgBgzp7)

Befehl:

~~~~
cd ~/Documents/Claude/boulder-v2 && sed -n 11,32p fakten-innen.md; sed -n 55,110p fakten-innen.md
~~~~

Ausgabe:

~~~~
### 1.1 Schichten und Algorithmus-Schnittstelle
- **Schnittstelle:** Es gibt eine austauschbare Algorithmus-Schnittstelle, U: include/pbl/services/activity/activity_algorithm.h:209-377. Sie hat 21 Funktionen:
  - init, early_deinit, deinit, set_user
  - handle_accel, minute_handler
  - get_steps, get_step_rate, metrics_changed_notification
  - enable_activity_tracking, activity_hrm_is_active, activity_hrm_set_paused
  - get_last_sleep_utc, send_minutes, post_process_sleep_sessions
  - get_minute_history, dump, minute_file_info und zwei Testfunktionen
- **Austausch nicht vorgesehen:** Die Doku sagt, ein anderer Algorithmus müsse nur diese API implementieren (U: docs/architecture/activity/index.md:205-209). Tatsächlich ist Kraepelin fest verdrahtet (U: fw/services/activity/CMakeLists.txt:12-13). Es gibt keine Kconfig-Wahl.
- **Der Glue tut mehr als rechnen:** U: fw/services/activity/kraepelin/activity_algorithm_kraepelin.c hat 1424 Zeilen. Er führt außerdem die Minutendatei und den Minuten-DLS (Tag 81), die Minutenhistorie für Apps und das Nullen der Schritte im Schlaf. Wer ihn komplett ersetzt, muss das alles nachbauen.
- **Veraltete Doku:** U: docs/architecture/activity/index.md:227-229 beschreibt, dass der Schlaf alle 15 min aus 36 h Flash neu gerechnet wird. Der Code tut das nicht mehr. Kraepelin rechnet den Schlaf heute fortlaufend Minute für Minute, mit einem rollenden Fenster von 9 Minuten (U: …/kraepelin_algorithm.c:1666-1725, 2120-2150). ACTIVITY_SESSION_UPDATE_MIN=15 nutzt nur noch eine Demo-App.

### 1.2 Wer ruft was, auf welchem Task
- **Schritte:** Die Accel-Daten laufen ISR → NewTimers-Work → accel_manager → `system_task_add_callback` → KernelBG `prv_accel_cb` (U: fw/services/accel_manager/service.c:264-265; U: fw/services/activity/activity.c:1109-1160).
  - Ein Paket hat 125 Samples, also alle 5 s ein Callback.
  - Bei laufender Vibration werden die Samples genullt (activity.c:1111-1113).
  - Ändern sich die Schritte, wird HealthEventMovementUpdate ausgelöst (activity.c:1147-1159).
- **Minute:** Ein `pbl_cron_job` läuft jede Minute auf NewTimers und reicht an KernelBG weiter: `prv_minute_system_task_cb` (activity.c:953-987). Ablauf:
  1. `activity_metrics_prv_minute_handler`
  2. `activity_algorithm_minute_handler`
  3. `_tail` mit Sitzungen, Speicher, Mitternacht, HR-/SpO2-Takt
  4. Insights (activity.c:860-972)
### 1.4 Kraepelin: Schritte, Gehen und Laufen
- **Schrittzählung:** 25 Hz, Epochen zu 125 Samples (5 s). Je Achse eine 128-Punkt-FFT, Betrag, Schrittfrequenz und eine „PIM“-VMC nach Actigraph-Art (kraepelin_algorithm.c:58, 1187-1259, 1308-1335).
- **Speicher:** KAlgState hat 1520 B (ELF; Upstream ohne unser movement_state ≈ 1492 B), AlgState 1344 B. Beides liegt im Kernel-Heap und wird beim Start des Trackings angelegt (activity_algorithm_kraepelin.c:777-818).
- **Gehen/Laufen** (kraepelin_algorithm.c:1961-1972, 2001-2115):
  - Gehen: 40–130 Schritte/min, Laufen: 130–255.
  - Eine Sitzung braucht mindestens 10 min und endet nach 7 inaktiven Minuten.
  - Nach 3 min Aktivität wird HR mit 1 Hz eingeschaltet; die Anmeldung läuft nach 10 min ab und setzt die Walk/Run-Szene (2041-2056, 384).
  - Sitzungen werden nur über `prv_create_activity_session_cb` gemeldet (activity_algorithm_kraepelin.c:133-175).
- **Zeitsprung:** Springt die Zeit um mehr als 5 min vor oder irgendwie zurück, werden die Detektoren zurückgesetzt. Ein laufender Schlaf ist dann weg (kraepelin_algorithm.c:2128-2132). Ein KernelBG-Stau von über 5 min hätte dieselbe Wirkung.
- **Genauigkeit:** Im Repo gibt es keine Zahl. Es gibt nur einen Prüfkorpus mit Min/Max je Probe (U: tests/fw/services/activity/test_kraepelin_algorithm.c:1237-1302). Die Doku nennt einen Hochfrequenzfilter gegen Autofahrten (index.md:118). Fahrrad erkennt Kraepelin als Gehen; das hat unser Fork beobachtet.

### 1.5 Kraepelin: Schlaf
- **Eingaben je Minute:** Nur VMC, Lage und „sicher nicht getragen“. Letzteres heißt: am Ladekabel oder ein HRM-Off-Wrist, das jünger als 15 min ist (activity_algorithm_kraepelin.c:947-965; activity_private.h:136). **Puls und HRV fließen nicht in den Schlaf ein.**
- **Score:** Gewichtete Faltung der VMC über 9 Minuten mit den Gewichten {10,15,28,31,85,15,10,0,0}/100 (kraepelin_algorithm.c:1264-1276).
- **Parameter** (kraepelin_algorithm.c:212-228):
  - Schlafminute bei Score ≤330
  - Einschlafen nach 5 Schlafminuten in Folge
  - Aufwachen nach 11 Wachminuten in Folge, in der ersten Stunde nach 14
  - Mindestens 60 min Schlaf
  - Sofort wach bei VMC >10000 oder Score >8000
  - Verworfen bei mehr als 89 % unruhigen Minuten oder Mittel-VMC >180
- **„Restful“ (Tiefschlaf-Ersatz):** 20 Minuten in Folge mit Score ≤160, höchstens 8 je Zyklus (kraepelin_algorithm.c:234, 270-274, 1555-1660).
- **Nicht getragen:** Heuristik über Lage und VMC mit 180 Minuten (304-317, 1388-1449).
- **Keine Phasen.** Es gibt kein REM und kein Wach im Block. Eine Nacht wird zu mehreren „Sleep“-Zyklen; laut activity.h:72-73 sind es meist 4–5 Sitzungen.
- Die laufende Sitzung wird ab 60 min als „ongoing“ gemeldet (1935-1949).
- **Nickerchen:** Schlaf von höchstens 3 h, der ganz zwischen 12 und 21 Uhr liegt, wird zum Nap (activity_algorithm_kraepelin.c:695-774; kraepelin/activity_algorithm_kraepelin.h:19-31).
- **Tagesgrenze 21:00:** Schlaf, der danach endet, zählt für den nächsten Tag (activity_private.h:70; activity_sessions.c:26-32, 448-461).
- **Qualität:** Reine Aktigrafie. Die Doku nennt nur Fixture-Grenzen (index.md:252-260). Einen Referenzvergleich gibt es nur in unserem Fork, und zwar für Boulders Analyse, nicht für Kraepelin.

### 1.6 Sitzungsliste (U: fw/services/activity/activity_sessions.c)
- **Kapazität:** Feste 32 Plätze für den ganzen Tag, geteilt von Schlaf, Bewegung und Training (activity.h:75). Ist kein Platz mehr frei, fällt eine Sitzung still weg (180-183).
- **Format:** ActivitySession hat 16 B, packed; step_data ist uint16, also höchstens 65535 Schritte je Sitzung (activity.h:337-389).
- **Persistenz:**
  - Das rohe Feld mit 512 B steht in der Settings-Datei „activity“ (16 KiB, activity_private.h:145-147). Es wird alle 15 min geschrieben, wenn sich etwas geändert hat (activity.c:799-853).
  - Ändert sich die Größe des Felds, wird alles verworfen (593-597).
  - **Beim Booten werden alle laufenden Sitzungen gelöscht** (632).
- **Abgleich:** Gleich ist, was Art und Start teilt. Bei Schlaf genügt der Start, egal welche Schlafart (138-152, 173).
- **Asserts und Fehler:**
  - Ein unbekannter Typ im Flash führt zu Nullen und WTF, also zu einem Neustart (614-627).
  - Ein unbekannter Typ in `is_sleep_activity` gibt WTF (95), ebenso in `prv_log_activities` (535).
  - Wer eine nicht laufende Sitzung löscht, trifft PBL_ASSERT (218).
- **Upstream-Fehler, die noch drin sind:**
  - Beim Entfernen kopiert `memcpy` überlappend (67).
  - Nach `memmove` wird der letzte Platz nicht genullt. Nach einem Neustart steht die Sitzung doppelt da (221-230).
- **DLS:** Sitzungen gehen auf Tag 84 (Satz v3, 26 B). Schlaf geht erst hinaus, wenn der Zustand „wach“ ist (238-279, 548-552).

### 1.7 Metriken und Schlafzustand (U: fw/services/activity/activity_metrics.c)
- **Historie:** 30 Tage (activity.h:67), Werte als uint32.
- **Schlafkennzahlen:** Summe und Restful werden aus den Sitzungen gerechnet (activity_sessions.c:302-349).
- **ActivitySleepState wird nur indirekt abgeleitet** (activity_sessions.c:395-415). „Schlafend“ gilt nur, wenn das letzte Sitzungsende höchstens 1 min vom Ergebnis von `activity_algorithm_get_last_sleep_utc()` abweicht (Kraepelin-Minute − 4 min). Sonst gilt „wach“. Wer diesen Zustand lesen:
  - Smart Alarm
  - health_service_peek_current_activities (U: fw/applib/health_service.c:1345-1356)
  - Insights
  - Zifferblätter
- **HR-Median je Minute:** Nach Qualität gewichtet, mit zwei task_zalloc je Minute (368-398).
- **HR-Zone:** Steht pro Minute in Zone ≥1 im Flash (401-424, 456).
~~~~
