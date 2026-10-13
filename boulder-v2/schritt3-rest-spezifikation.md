# Boulder V2 – Schritt 3, Rest: AP6, AP8, AP9

Stand 12.10.2026, Fassung 1. Ersetzt für AP6, AP8 und AP9 die verlorene `schritt3-spezifikation.md`
(Fassung 2 mit „Gegenprüfung 2“, 08.10.). Der Code steht auf `origin/v2` bis `4fefab43`.

**Quellenlage.** Die Fassung 2 ist mit dem alten PC verloren. Was hier steht, kommt aus:
- **[E]** `entscheidungen.md` (vollständig gerettet), Abschnitt „Schritt 3“ mit F1–F10;
- **[A]** den Auszügen der Fassung vom 08.10. 09:01 (`auszuege/boulder-v2/schritt3-spezifikation.auszuege.md`,
  2.1–2.4, 8, 10, 11) – das ist der Stand **vor** den Entscheiden F1–F10, darum hier nachgeführt;
- **[W]** den geretteten Workflow-Skripten (`v2-schritt3-ueberarbeiten.js`, `v2-schritt3-bau.wf.js`);
- **[C]** den Commit-Texten von AP0–AP5 und AP7 und dem Code auf `origin/v2`;
- **[N]** neu festgelegt in dieser Fassung. Alles mit [N] steht zur Bestätigung (Abschnitt 8).

---

## 0. Kurzfassung

- **AP6** baut die fünf Karten und ihre Details mit Inhalt, das Laden beim Start und das
  Auffrischen im Minutentakt. Daten kommen nur aus S1–S7 und wenigen Pebble-Bausteinen, die AP1
  geprüft hat. Kein Minutenverlauf, keine Datei, kein Telefon.
- **AP8** misst in QEMU die Ladeziele L1–L6, den Speicher und die Stabilität. Die Messskripte
  der alten Sitzung sind verloren; sie kommen diesmal ins Repo (`tools/boulder/`).
- **AP9** führt zusammen, prüft die Kriterien von Schritt 1–3 und schreibt `schritt3-abnahme.md`.
- **Neu zu entscheiden:** Today ohne Detail, Night-Detail ohne „Last nights“, Kartenfarben,
  Abnahmekriterien von Schritt 3 und die Richtung der Taste Oben (Abschnitt 8).

---

## 1. Was schon steht (AP0–AP5, AP7) [C]

| Teil | Wo | Was es kann |
|---|---|---|
| App-Rahmen | `fw/apps/system/boulder_health/boulder_health.c` | ID −82, UUID und `health_app_get_info` von Pebble, ohne Systemrechte, Zustand im Heap der App, weisses Fenster, Oben/Unten blättern |
| Einstiege | `einstieg.c/.h` | erste Karte je Startgrund, Kreis, Spiegel, Weg zum Zifferblatt (2.4), ohne Syscall |
| Kartenrahmen | `karten.c/.h` | Titel und Kopfbalken (0–27 px, GOTHIC 18 fett, fünf Punkte), noch eine Farbe für alle (`GColorSunsetOrange`, 3,1:1) |
| Rechnungen | `rechnung.c/.h` | Schlaf-, Ruhepuls- und HRV-Urteil, HRV-Woche mit Band, Zonen und Zonenfarben, Formate; rein, gegen Boulders Kotlin geprüft |
| Schnittstelle | `include/pbl/services/activity/boulder/schnittstelle.h` | S1 Nacht, S2 Lage (mit Zielen, Messstufe, Ruhepuls-Median, Maxpuls, SpO2-Takt), S3 SpO2, S4 Messstufe setzen, S5 Nächte (37 Wachtage HRV/Ruhepuls), S6 Herzband 24 h und Zonen heute, S7 Schritte der 6 Vortage |
| Ziele | `fw/services/activity/boulder/ziele.c` | `boulder_ziele` aus der BlobDB Health, Vorgabe 10 000 Schritte und 480 min |
| Zeitstempel | `zeiten.h` | `BH_ZEIT`/`BH_PROTOKOLL` → „bh t=<ticks> …“, nur mit `CONFIG_BOULDER_HEALTH_ZEITEN` (nur QEMU) |
| Probe | `probe.c`, `fehlerprobe.c` | jeder erlaubte Baustein einmal, nur im Prüfbau |
| Prüfwerkzeuge | `tools/boulder/` | `k1_waechter.py`, `pruefe_ohne_rechte.py` (rote Liste L5), `kopie_pruefen.py`, `vertraege.py` |
| Tests | `tests/fw/apps/system_apps/boulder_health/` | `test_bh_app`, `test_bh_einstieg`, `test_bh_einstiege`, `test_bh_rechnung`, `test_bh_startmenue` |

**Befund aus AP1, gilt für AP6:** Eine MenuLayer braucht `get_cell_height` und eigenes Zeichnen.
`menu_cell_basic_draw` und die Vorgabe-Zeilenhöhe lesen für eine App mit System-ID Kernel-RAM, und
die App endet daran (`pruefe_ohne_rechte.py`, `SYSTEM_ID`).

---

## 2. Bildschirme (nach den Entscheiden vom 08.10.)

### 2.1 Aufbau [A, E]

- Fünf Karten, jede füllt den Schirm (200×228). Reihenfolge: **Today → Night → Heart → Activity → Settings**.
- Kopfbalken 0–27 px in der Farbe der Karte, Titel GOTHIC 18 fett weiss, fünf Punkte rechts (steht).
- Weisser Grund, Zahlen in LECO 26–38, Labels GOTHIC 14 grau. Fehlt ein Wert: „–“ in GOTHIC, „-“ in LECO.
- Urteile farbig: grün für „as usual“, „Well rested“, „in range“, sonst Bernstein.
- **Keine Sätze, kein Onboarding, kein Modal** (V8). Tracking aus: „Tracking off“ unter dem Titel, überall „–“.
- **Grundsatz [E]:** Jede Zahl stammt von der Uhr. Kein Koffein, keine Getränke, keine Präparate,
  keine Körperwerte, nichts vom Telefon ausser den Zielen.
- **Kartenfarben [N, F14]:** Today `GColorOrange` (3,2:1), Night `GColorIndigo` (10,7:1),
  Heart `GColorFolly` (3,9:1), Activity `GColorDarkGreen` (9,1:1), Settings `GColorDarkGray` (7,5:1).
  Today, Night und Heart wie b25. Die Zahl ist der Kontrast von Weiss auf der Farbe; der Render-Test
  prüft ≥ 3:1.
- Phasenfarben wie b25 und Boulder: Wach `ChromeYellow`, REM `PictonBlue`, Light `BlueMoon`, Deep `OxfordBlue`.

### 2.2 Karten

| Karte | Inhalt (y in px) | Quelle |
|---|---|---|
| **Today** | 32–74 **Sleep** „7:12 h“, rechts „Well rested“ / „Short night“ / „Very short night“ nach dem Schlafziel.<br>76–118 **Resting** „54 bpm“, rechts „as usual“ / „+3“ / „−3“ (ab 3 früheren Nächten).<br>120–162 **HRV** „48 ms“, rechts „in range“ / „above“ / „below“ (Band ab 3, Urteil ab 7 Werten).<br>164–220 **Steps** „6,230“ und Balken bis zum Schrittziel | S1, S2 (Ziele), S5, `ActivityMetricStepCount` |
| **Night** | 32–78 Dauer in LECO 38, rechts das Urteil, bei unfertiger Nacht „so far“.<br>80–100 „22:14 – 06:00“ (12/24 h).<br>106–166 die Nacht als **ein Block**, vier Bahnen Awake/REM/Light/Deep; Schlaf ohne Phase in Light.<br>172–222 vier Spalten mit den Minuten je Phase; ohne REM/Light/Deep entfällt die Zeile.<br>Ohne Nacht: „No night“ | S1, S2 |
| **Heart** | 32–80 Puls jetzt in LECO 38 mit „bpm“, rechts „now“ (≤ 15 min) oder das Alter („42 min“).<br>86–150 **Pulsband 24 h**: 48 Halbstunden-Säulen zu 4 px, Minimum bis Maximum grau, darüber die Linie der Mittel in der Zonenfarbe, Nacht als Schatten.<br>156–222 drei Spalten Resting, HRV, SpO2 (letzter Wert in 24 h; „off“, wenn die Uhr SpO2 nicht misst).<br>Titel immer „Heart“ [E] | `ActivityMetricHeartRateRawBPM` und `…RawUpdatedTimeUTC`, S6, S2 (Grenzen), S1 (Schatten), S5, S3 |
| **Activity** | 32–80 Schritte in LECO 38. 84–90 Balken bis zum Ziel.<br>96–118 „4.8 km · 42 min active“ (km/mi nach Pebbles Einstellung).<br>126–222 **Woche**: 7 Säulen, 6 Vortage aus S7 und heute, Wochentag in zwei Buchstaben, heute hervorgehoben, Ziel als Linie. Ein Tag ohne Wert bleibt leer | `ActivityMetricStepCount`, `…DistanceMeters`, `…ActiveSeconds`, S7, S2 |
| **Settings** | 32–100 „Precision“, darunter der Name der Stufe („Balanced“) und fünf Punkte | S2, Namen aus `messstufe.h` (Flash) |

### 2.3 Details (Select)

| Von | Detail | Inhalt | Quelle |
|---|---|---|---|
| Today | **keins [N, F12]** | Select tut nichts. Das Tagesband entfällt (F3); seine Teile stehen auf Night, Heart und Activity | – |
| Night | **Night**, scrollt | **THROUGH THE NIGHT:** Block mit Phasen, darunter der Puls der Nacht aus den Halbstunden von S6 und die SpO2-Punkte der Nacht auf einer Zeitachse.<br>**BODY DURING SLEEP:** Resting und HRV mit „avg“ der früheren Nächte, SpO2 mit „lowest“.<br>„Last nights“ entfällt [N, F13] | S1, S6, S3, S5 |
| Heart | **Heart**, scrollt | **ZONES TODAY:** Minuten in Light, Moderate, Vigorous, Peak.<br>**SPO2 24 H:** Punkte.<br>**HRV:** 7 Nächte mit Band (`bh_hrv_woche`) | S6, S3, S5 |
| Activity | **Sessions** | Liste der Sitzungen von heute: Symbol, Art, Beginn, Dauer, Schritte oder kcal. Walk/Run erkannt, Ride (Open, nicht manuell), Workout (manuell), Nap. Sleep und Restful nicht.<br>Unten km, aktive Minuten, kcal des Tages (V12: Pebbles Summe) | `sys_activity_get_sessions`, Metriken |
| Settings | **Precision** | MenuLayer mit den 5 Stufen (Battery Saver, Light, Balanced, Detailed, Precise), Häkchen bei der geltenden. Select setzt sie über S4 und schliesst die Liste. Eigene Zeilenhöhe und eigenes Zeichnen (Befund AP1) | S2, S4 |

### 2.4 Navigation [A, C]

Steht seit AP7 und bleibt: Oben/Unten blättern wie Pebble (Oben = nächste Karte), Kreis aus dem
Startmenü, nach Quick Launch Oben/Unten kein Kreis und gespiegelt, Pins öffnen Night (Typ 1) oder
Activity (Typ 0), Start vom Telefon Today. Wischen kommt von Pebble für ID −82.

Neu in AP6: **Select** öffnet das Detail (auf Today nichts), **Zurück** im Detail führt zur Karte, auf der
Karte schliesst es die App. Im Detail scrollen Oben/Unten. Die Richtung der Taste Oben ist F11.

### 2.5 Texte [N]

Alle sichtbaren Texte stehen in **einer** Tabelle `texte.c/.h` (wie Kieselsport), Englisch. Gruppen: Titel,
Labels, Urteile, Einheiten, Leer- und Sonderfälle. Formate (Zahlen, Dauer, Uhrzeit) liefert `rechnung.c`.
Ein Test prüft, dass jeder Text auf der breitesten Karte in seine Fläche passt (GOTHIC 14/18, LECO).

### 2.6 Laden und Auffrischen

#### 2.6.1 Warum b25 langsam war [C, belegt am Code von `main`]

`src/fw/apps/system/boulder_health/daten.c:41-45` las die Minutenhistorie mit
`health_service_get_minute_history` stückweise aus dem Flash, 24 Aufrufe je Tag. Das passierte beim Start
und jede Minute; der Kommentar in `daten.h` beschreibt es selbst („spürbar spät erschien und jede Minute kurz
stand“). Dazu las `boulder_nacht_letzte` beim ersten Aufruf eine Datei auf dem Stapel der App (`daten.c:141`).

**Folge für V2:** Die Variante führt Herzband, Zonen, Nächte und Woche laufend im Kern mit (F8, F9, AP3, AP4).
Die App liest nur kleine Kopien über S1–S7. Minutenverlauf, `persist_*` und `app_message_*` stehen auf
der roten Liste (L5).

#### 2.6.2 Ladeweg [N, aus W und C abgeleitet]

1. **Phase 0 (steht):** Startgrund, Taste, Quick-Launch-Aktion und Argument einmal lesen, erste Karte wählen.
2. **Phase 1, vor dem ersten Bild:** einmal S2, S1, S5, S6, S7, S3 (24 h) und die Metriken des Tages in den
   Heap der App. Was ein Syscall nicht hat (S1 „später“, S5/S6/S7 `false`), zeigt „–“ und kommt in Phase 2.
3. **Phase 2:** Hat S1 „später“ gesagt oder fehlte S5/S6/S7, fragt ein `app_timer` nach 1 s noch einmal,
   höchstens 5-mal.
4. **Tasten** lesen nichts: Karten und Details zeichnen aus dem Zustand im Heap (L3).
5. **Auffrischen** im Minutentakt (`tick_timer_service`, MINUTE_UNIT): S2, S6, S7, die Metriken, S3 neu,
   dann die gezeigte Karte neu zeichnen. S1 und S5 nur, wenn die Lage eine neue Nacht meldet
   (`nacht_stufe` oder `fenster_utc` geändert).
6. **Puls live (V3):** Nur auf der Karte Heart und höchstens 2 min `health_service_set_heart_rate_sample_period(1)`;
   beim Verlassen der Karte, nach 2 min und beim Ende der App zurück auf 0. Health-Ereignisse frischen nur
   die Zahl „Puls jetzt“ auf.
7. **Ende:** Timer, Tick und Health-Abo abmelden, Heap freigeben. Kein Abo überlebt die App.

#### 2.6.3 Zeitstempel und Ladeziele

Stempel (stehen): `main`, `start …`, `bild`, `taste`, `karte …`, `zifferblatt`, `nichts`.
Neu [N]: `daten` (Ende Phase 1), `nachgeladen <syscall>` (Phase 2), `auffrischen` und `auffrischen fertig`,
`detail <name>`, `puls an` / `puls aus`.

| Ziel | Inhalt | Messung | Status |
|---|---|---|---|
| **L1** | Start: `main` → erstes `bild` **mit Werten** ≤ 50 ms | QEMU, 20 Starts je Einstieg, Höchstwert | Zahl aus [W], Nummer rekonstruiert |
| **L2** | Kartenwechsel: `taste` → `bild` ≤ 30 ms, auch für Details | QEMU, jede Karte, 20 Mal | [W] |
| **L3** | Eine Taste löst keinen Syscall aus | `test_bh_app__tasten_ohne_syscall` (steht, wird für den Minutentakt erweitert) | [C] |
| **L4** | Gelesen wird einmal je Start und dann nur im Minutentakt; Auffrischen `auffrischen` → `bild` ≤ 30 ms | QEMU und Host-Test (Syscalls je Minute zählen) | [W] „Cache je App-Start“, Zahl [N] |
| **L5** | Rote Liste: kein Minutenverlauf, kein `persist_*`, kein `app_message_*` | `pruefe_ohne_rechte.py` | [C] |
| **L6** | Jeder Boulder-Syscall ≤ 1 ms, ohne Datei, Heap oder Warten | QEMU (Ticks um jeden Syscall im Prüfbau), Host-Tests mit `--wrap` (stehen) | [W, C] |

QEMU ist nicht obelix: Die Zahlen gelten als Nachweis der Bauart. Auf der Uhr misst der Gerätetest dieselben
Stempel noch einmal mit einem Prüfbau für obelix ohne `ZEITEN` (Stoppuhr am Video, Abschnitt 6).

---

## 3. AP6 Bildschirme und Laden

**Inhalt:** 2.1–2.6 umsetzen. Dateien in `fw/apps/system/boulder_health/`: `daten.c/.h` (Zustand und
Laden), `karten.c` (fünf Karten), `details.c/.h` (vier Details), `texte.c/.h`, `boulder_health.c`
(Select, Zurück, Timer, Tick, Puls live). Kein Pebble-Code, kein beschreibbares `static`.

**Tests (Host):**
- `test_bh_karten`: jede Karte und jedes Detail **voll, leer, Tracking off, ohne Nacht, 12 h und 24 h,
  km und mi**, gegen Bild-Fixtures; Kontrast jedes Kopfbalkens ≥ 3:1; jeder Text passt (2.5).
- `test_bh_daten`: Phase 1 und 2 mit gefälschten Syscalls (S1 „später“, S5–S7 `false`, dann da),
  höchstens 5 Nachfragen, Auffrischen nur aus dem Tick, S1/S5 nur bei neuer Nacht.
- `test_bh_app` erweitern: Select/Zurück, Details, L3 mit Tick, Puls live an und aus (Karte verlassen,
  2 min, App-Ende), kein Abo nach dem Ende.
- **Grundsatz-Test:** Jede Zahl auf jedem Bild kommt aus einer Quelle der Tabellen 2.2/2.3. Der Test
  füllt jede Quelle mit einem eigenen Erkennungswert und sucht ihn im Bild.
- Mutationen je Kernaussage (Urteilsfarbe, Ziel aus der Lage, Kein-Syscall-bei-Taste, Puls-Abo zurück,
  Tracking off, „off“ bei SpO2).

**Prüfweg:** obelix, qemu_emery, obelix Release und Prüfbau ohne Warnung; `pruefe_ohne_rechte.py`
grün (ausser der Fehlerprobe im Prüfbau); K1-Wächter grün; Host-Tests alle grün.

**Belege:** QEMU-Bilder jeder Karte und jedes Details nach `boulder-v2/belege-schritt3/` im Zweig `notizen`.

---

## 4. AP8 QEMU, Ladezeit, Ressourcen

**Prüfbau:** `pbl configure --board qemu_emery -DCONFIG_BOULDER_PRUEFLAUF=y -DCONFIG_BOULDER_HEALTH_ZEITEN=y`.

**Neu im Repo [N]:** `tools/boulder/qemu_health.py` startet den Prüfbau, bedient Tasten über den Monitor,
liest „bh t=…“ von der Konsole (TCP 12345), macht Bilder über den Socket-Monitor und schreibt eine Tabelle.
Die Skripte der alten Sitzung (`ap2-qemu.py` u. a.) sind verloren; diesmal liegt alles im Repo.

**Messen und Prüfen:**
1. L1, L2, L4, L6 nach 2.6.3, je Wert Höchstwert und Median.
2. Tiefstand des App-Heaps über alle Karten und Details.
3. Kernel-Heap vor und nach 100-mal Öffnen und Schliessen: gleich.
4. Puls-Abos nach dem Schliessen: keins (auch nach Schliessen auf Heart während Puls live).
5. Längster Callback der App (Zeichnen, Tick, Timer).
6. Kein Neustart über den ganzen Lauf; Fehlerprobe am Endstand beendet nur die App.
7. Syscall-Stapel der App: Tiefstand mit allen Karten und Details (bisher 1 368 B frei mit S1–S7).
8. Kernel-RAM obelix Release gegen AP4: `.kernel_bss`, `.kernel_data` (soll unverändert, die App liegt im App-Heap).
9. **Ballast des Prüfbaus nachführen:** `CONFIG_BOULDER_PRUEFLAUF_HEAP` steht auf 71 200 B, obelix Release
   hat nach AP4 72 736 B. Auf den Wert nach AP6 setzen, sonst misst QEMU mit zu wenig Heap.

---

## 5. AP9 Integration und Abnahme

- Alle sechs Boards, obelix Release, Prüfbau ohne Warnung. Voller Host-Testlauf.
- **Baseline:** In der Cloud mit SDK 0.1.10 laufen alle 404 Tests grün (12.10., mit `vm.max_map_count`
  aus dem Start-Hook). Die alte Baseline „13 bekannte Fehler“ galt für SDK 0.1.8 auf dem alten PC. Neue
  Baseline: **0 Fehler**.
- Kriterien von Schritt 1 und 2 erneut (Wächter, Prüfnächte, Kieselsport-Tests), Kriterien von Schritt 3
  (Abschnitt 7) mit Belegstand: erfüllt, teilweise oder offen.
- Mutationen: eine bis zwei je Kriterium.
- Bericht `boulder-v2/schritt3-abnahme.md` im Zweig `notizen`, Belege in `belege-schritt3/`.
- Am Ende die Liste bis zur ersten Vorabversion (Abschnitt 6).

---

## 6. Nach Schritt 3: bis zur ersten Vorabversion [A, E]

1. Upstream v4.40.0 nachziehen; Probe ohne Systemrechte danach wiederholen (MPU und Syscalls umgebaut).
2. Wechsel b26 → V2 testen (bisher mit einem b25-Abbild).
3. Boulder 0.53: Seite „Goals“ (schreibt `boulder_ziele`), V2 als Firmware erkennen, ~~Gravel 0.3.4~~
   (gestrichen am 13.10.: Boulder baut nicht mehr auf Gravel auf).
4. Kieselstrasse nach V2 portieren (F6).
5. Vorher-Video von b26 (braucht deine Uhr).
6. Gerätetest: 7 Nächte, Fitbit-Nächte, Velo- und Autofahrten, Messstufen, Ladezeiten auf der Uhr.

---

## 7. Abnahmekriterien Schritt 3 (Entwurf, neu) [N, F15]

Die 8 Kriterien der Fassung 2 sind verloren und waren nicht in `entscheidungen.md` bestätigt. Neuer Entwurf:

1. **Pebble-Code** nur P7 (4 Zeilen in `fw/apps/system/CMakeLists.txt`); K1-Wächter grün.
2. **Ohne Systemrechte:** keine beschreibbaren `static`, nur erlaubte Aufrufe, rote Liste leer
   (`pruefe_ohne_rechte.py`); die Fehlerprobe beendet nur die App.
3. **Grundsatz:** Jede Zahl stammt von der Uhr; nichts vom Telefon ausser den Zielen; kein Koffein,
   keine Getränke, keine Präparate.
4. **Gleiche Urteile wie Boulder** am selben Morgen (Schlaf, Ruhepuls, HRV, Ziele): Orakel-Tests 0 Abweichungen.
5. **Einstiege:** Startmenü, Quick Launch (alle Gesten), Pins, Start vom Telefon landen auf der richtigen
   Karte; andere Boards behalten Pebbles App.
6. **Ladezeit:** L1–L6 erfüllt in QEMU; auf der Uhr im Gerätetest bestätigt.
7. **Ressourcen:** kein Kernel-Heap-Verlust nach 100 Starts, kein Puls-Abo nach dem Ende, kein Neustart.
8. **Ziele:** `boulder_ziele` vom Telefon gilt auf Uhr und in Boulder; ohne Telefon der letzte Wert oder die Vorgabe.

---

## 8. Zu entscheiden

**F11 Taste Oben.** In V2 blättert Oben wie bei Pebble zur nächsten Karte, in b25 zurück.
- A: wie Pebble (so gebaut). Nach Quick Launch über Oben führt die Gegenrichtung dorthin, wohin Pebbles
  Shutter beim Schliessen fällt.
- B: wie b25. Dann passt die Richtung nach Quick Launch nicht mehr zum Übergang der Shell.
- **Empfehlung A.**

**F12 Today ohne Detail.** Das Tagesband war das Detail von Today und entfällt (F3).
- A: kein Detail, Select tut auf Today nichts.
- B: Select auf Today öffnet das Detail der Zeile, die am meisten sagt (Night-Detail).
- **Empfehlung A.** Einfach, keine doppelte Seite; die Details stehen auf ihren Karten.

**F13 Night-Detail ohne „Last nights“.** Mit F2 speichert die Uhr nur HRV und Ruhepuls früherer Nächte,
nicht ihre Dauer.
- A: „Last nights“ streichen.
- B: stattdessen Ruhepuls der 7 Nächte zeigen (doppelt zu Heart, dort HRV).
- **Empfehlung A.**

**F14 Kartenfarben** wie in 2.1 vorgeschlagen (Today, Night, Heart wie b25; Activity Dunkelgrün; Settings Grau).
- **Empfehlung: so.**

**F15 Abnahmekriterien** von Schritt 3 wie in Abschnitt 7.

**F16 Ladeziele.** Die Nummern L1–L6 sind aus Code und Bau-Skript rekonstruiert; die Zahlen für L1, L2
und L6 stehen im geretteten Bau-Skript. Neu ist nur die Zahl für L4 (Auffrischen ≤ 30 ms).
- **Empfehlung: so übernehmen.**

---

## Stand nach AP6 (12.10.2026, `0c1b4736`)

- Gebaut nach dieser Spezifikation. Abweichungen (im Commit begründet):
  - Tracking aus: mittig „Tracking off“ statt eines Strichs in jedem Feld; Settings bleibt bedienbar.
  - Die Nacht im Pulsband ist ein Streifen unter dem Band, kein Schatten.
  - Precision ist eine eigene Ebene statt einer MenuLayer.
  - L3 genauer: Eine Taste liest keine Daten; geschrieben wird an einer Taste nur der Puls-Takt (V3)
    und die Messstufe (S4).
- Prüfung: Host 407/407, Mutationen 15/15 rot, acht Bauten ohne Warnung, Heap obelix Release 72 736 B.
- Korrektur zum Commit-Text: `test_bh_daten` hat 10 Fälle, nicht 11.
- Belege: `belege-schritt3/ap6-qemu/`. Vorläufige Emulator-Skripte: `werkzeug/` (kommen mit AP8 als
  `tools/boulder/qemu_health.py` ins Repo).
- Nächster Schritt: AP8 (Abschnitt 4).

## Stand nach AP8 (12.10.2026, `d213119c`)

- Messung mit `tools/boulder/qemu_health.py`, alle Ziele erfüllt (QEMU):
  - L1: Median 7–8 ms, höchstens 32 ms (Ziel 50 ms).
  - L2: Karte höchstens 13 ms, Detail höchstens 17 ms (Ziel 30 ms).
  - L4: höchstens 8 ms (Ziel 30 ms).
  - L6: rund 10 µs je Syscall (Ziel 1 ms).
- Ressourcen: App-Heap höchstens 5,4 KB; Kernel-Heap nach 100 Starts ohne Wachstum (einmalig 68 B);
  kein Puls-Abo nach dem Ende; Syscall-Stapel 1 368 B Reserve; kein Neustart, die Fehlerprobe
  beendet nur die App.
- Behoben, was die Messung fand: doppeltes Lesen beim Start (Pebbles erster Takt), Details erst
  nach Pebbles Schiebe-Animation (jetzt ohne Übergang), doppeltes Layout je Text.
- Ballast-Ziel des Prüfbaus jetzt 72 736 B.
- Belege: `belege-schritt3/ap8-qemu/` (messung.json, Protokoll, Bilder).
- Nächster Schritt: Gegenprüfung von Schritt 3, dann AP9 (Abschnitt 5).

## Stand nach AP9 (12.10.2026, `ecb6b739`)

- Abnahme: `schritt3-abnahme.md`. Kriterien 1–5 und 7 erfüllt; 6 und 8 teilweise (offen sind der
  Gerätetest und die Seite „Goals“ in Boulder 0.53).
- Gegenprüfung: drei Prüfer, 23 Befunde (21 verschiedene), alle behoben in `11f0bb2a`, `082171a9`,
  `022dd797`, `13e7a021` und `55d8e4c9`. Darunter K1 (S3 schrieb mit Systemrechten hinter das App-RAM)
  und A1 (Use-after-free).
- AP9: Das Messskript setzt die Schritte erst nach Pebbles Tageswechsel (`ecb6b739`). Der erste Start
  nach dem Boot schwankt in Echtzeit-QEMU mit dem Host (30–54 ms); mit `-icount` sind es 17,6 ms wie in
  AP8. Neu zu entscheiden: F17 (Abnahme, Abschnitt 7).
- Prüfung: acht Bauten ohne Warnung, Host 409/409, Werkzeuge grün, drei QEMU-Läufe grün; Mutationen
  43/43 rot, dazu die QEMU-Mutation.
- Belege: `belege-schritt3/ap9/`.
- Nächster Schritt: Abschnitt 6 (bis zur ersten Vorabversion), Abnahme Abschnitt 7.

## Nachtrag 13.10.2026: Schlafrechnung von V2

- Nach den Nächten vom 7. bis 12.10. weicht die Schlafrechnung von V2 von b26 ab. Ein Wegnicken vor
  einem Aufwach-Schub ist kein Einschlafen, und das Wälz-Fenster ist 30 statt 40 Minuten (`12b28664`,
  `4ea6fb64`).
- Analyse: `naechte/analyse-2026-10-13.md`; Entscheid: `entscheidungen.md` (13.10.).
- `test_boulder_schlafanalyse_wie_b26` ist ersetzt durch `test_boulder_schlaf`. Die Vergleiche mit b26
  auf den Referenznächten bleiben.

## Stand nach Upstream v4.40.0 (13.10.2026, `0510648c`)

- Punkt 1 aus Abschnitt 6 erledigt: v4.40.0 zusammengeführt, ohne Konflikt.
- Nachgezogen:
  - Testpfade (uzlib, upng, `drivers/`).
  - `umzug_pruefen.py`: `start_utc` ist jetzt `int32_t`, auf der Uhr gleich wie `time_t`.
  - `HERKUNFT` nennt v4.40.0.
- Ohne Systemrechte, QEMU und `pruefe_ohne_rechte` wiederholt; alles grün bis auf F17.
  Upstreams neue Syscall-Prüfung ist grün.
- Prüfung: acht Bauten ohne Warnung, Host 433/433, Werkzeuge grün, Mutationen 43/43 rot.
- Bericht: `schritt3-abnahme.md`, Abschnitt 8. Belege: `belege-vorab/upstream-v4.40.0/`.
- Neu zu entscheiden: F18 Upstream v4.41.0 (Abnahme, Abschnitt 7). Nächster Schritt danach: Punkt 2
  (Wechsel b26 → V2).

## Stand nach Upstream v4.41.0 (13.10.2026, `273728a3`)

- F17 B umgesetzt (`f2753ba4`): Der erste Start nach dem Boot zählt unter `-icount`.
- F18 A: v4.41.0 nachgezogen. **Der Upstream-Stand ist jetzt bis nach dem Gerätetest eingefroren.**
- Nachgezogen:
  - Konflikte an sortierten Includes.
  - Die Kraepelin-Kopie per `git merge-file`.
  - C23-Attribute und `-Werror` im Host-Bau.
  - Ein eigener Stub für den Beschleunigungsdienst.
  - Der Weckertest: Umstellen beendet jetzt den laufenden Smart Alarm (Upstream).
- Prüfung: acht Bauten ohne Warnung, Host 437/437, Werkzeuge grün, Mutationen 43/43 rot, QEMU
  zweimal grün.
- Bericht: `schritt3-abnahme.md`, Abschnitt 9. Belege: `belege-vorab/upstream-v4.41.0/`.
- Nächster Schritt: Punkt 2 aus Abschnitt 6 (Wechsel b26 → V2).

## Stand nach dem Wechseltest b26 → V2 (13.10.2026, `b6a4b3df`)

- Punkt 2 aus Abschnitt 6 erledigt. Der echte Wechsel in QEMU hat alles übernommen und nichts
  verdoppelt; die Wechselprüfung vergleicht jetzt mit b25 und b26.
- Nebenbei nachgeholt: Die ganze Prüfung läuft jetzt mit SDK 0.1.11 (GCC 15), wie v4.41.0 es
  verlangt.
- Bericht: `schritt3-abnahme.md`, Abschnitt 10 (Wechsel) und Abschnitt 9, Nachtrag (SDK).
- Nächster Schritt: Punkt 3 (Boulder 0.53: Seite „Goals“, V2 erkennen, Gravel 0.3.4) oder Punkt 4
  (Kieselstrasse nach V2).

## Stand nach Boulder 0.53 (13.10.2026, Boulder `aecd3350`, Zweig `claude/exciting-cerf-m8apu2`)

- Punkt 3 aus Abschnitt 6 gebaut: Seite „Goals“ mit `boulder_ziele`, V2 als Firmware erkannt (ohne
  TimeStyle Bedrock). Gravel 0.3.4 gestrichen: Boulder baut nicht mehr auf Gravel auf.
- 2143 Tests grün, Release-Build grün. Bericht: `schritt3-abnahme.md`, Abschnitt 11.
- Offen: das Release 0.53.0 (Stand auf `main` bringen, die CI legt `v0.53.0` an).
- Für V2: Tag `v4.41.0-bN`, sonst gilt sie in Boulder als unbekannte Firmware.
- Nächster Schritt: Punkt 4 (Kieselstrasse nach V2).
