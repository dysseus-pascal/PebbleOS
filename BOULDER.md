# Boulder-Firmware

Fork von PebbleOS (Core Devices, Apache 2.0) ab **v4.38.1**, nachgezogen auf
**v4.38.4**, für die Begleit-App
[Boulder](https://github.com/dysseus-pascal/Boulder). Ziel: Pebbles Aufbau
behalten (die Uhr misst dauernd, legt Minutenwerte ab, erkennt Abschnitte und
schickt alles per Datalogging), aber mit unseren Algorithmen und Apps.

## Leitlinien

- **Auswertung auf der Uhr.** Schritte, Schlaf mit Phasen, HRV, Ruhepuls und
  SpO2 rechnet die Firmware. Das Telefon empfängt, zeigt an, schreibt nach
  Health Connect und hält eine Referenzrechnung zum Nachprüfen.
- **Training nur, wenn selbst gestartet** (wie Garmin). Alltagsbewegung sind
  Schritte und aktive Minuten; erkannte längere Bewegung wird ein Abschnitt
  „Bewegung“, kein Training.
- **Eigene System-Apps** statt Health und Workout von Pebble. Die Sports-App
  (für Telefon-Apps wie Strava) bleibt.

## Schritte

| | Inhalt |
|---|---|
| F0 | Grundlage: Werkzeuge, Bau beider Slots, Host-Tests, QEMU |
| F1 | Erkannte Gehphasen werden „Bewegung“ statt Walk/Run; Kennzeichen „selbst gestartet“ |
| F2 | Eigener Schlafalgorithmus mit Phasen |
| F3 | Genauere Nacht: Puls jede Minute, HRV, Minutendaten v14 (SpO2 im Satz) |
| F4 | Eigene Health- und Trainings-App, Workout- und Health-App entfernt |
| F5 | Updates über Boulder aus den Releases dieses Forks |

## Bauen

Einmalig: PebbleOS-SDK 0.1.8 nach `~/pebbleos-sdk-0.1.8`, `python3 -m venv .venv`,
`.venv/bin/pip install -r requirements.txt`, `git submodule update --init --recursive`.

```
tools/boulder/bundle.sh            # beide Slots, zusammengeführt wie ein Release
```

Host-Tests des Aktivitätsdienstes (nur diese bauen; ein Bluetooth-Test
kompiliert mit neuerem clang nicht):

```
cd build-test && ninja fw/services/activity/test_kraepelin_algorithm/runme && ctest -R kraepelin
```

Host-Tests des Trainingsdienstes:

```
cd build-test && ninja fw/services/kieselsport/test_kieselsport/runme && ./fw/services/kieselsport/test_kieselsport/runme
```

## Schalter (Kconfig)

Was Boulder an Pebbles Schlaf, Puls- und SpO2-Takt und an der Health-App
ändert, hängt an vier Schaltern (`src/fw/services/activity/boulder/Kconfig`,
eingehängt in `services/Kconfig`). Alle an: Verhalten wie bis b24, mit einer
bewussten Ausnahme (SpO2 „off“ bei „Disabled“, siehe Health-App). Alle aus:
Schlaf, Takte, Einstellungen „Health“ und Health-App wie Upstream.

| Schalter | hängt an | gesetzt | bewirkt |
|---|---|---|---|
| `BOULDER_SCHLAF` | `SERVICE_ACTIVITY`, zieht `SERVICE_HEALTH_SYNC_ENDPOINT` | `prj_normal.conf` | Nacht und Schlafanalyse (`nacht.c`, `schlafanalyse.c`), Kraepelins Schlafsitzungen fallen weg, Schlafsumme ohne Wachminuten im Block |
| `BOULDER_HRV` | `BOULDER_SCHLAF && HRM` | `default y` | HRV-Fenster in der Nacht (`hrv.c`), Satz auf `0x4002` |
| `BOULDER_MESSSTUFE` | `BOULDER_SCHLAF && HRM` | `default y` | Regler „Precision“ (Pref `boulderMessstufe`) für Puls-, SpO2- und HRV-Takt; aus: Pebbles „HR Monitoring“ und „SpO2 Monitoring“ |
| `BOULDER_HEALTH_APP` | `BOULDER_SCHLAF && PLATFORM_EMERY` | `default y` | Boulders Health-App mit SpO2-Ring (`spo2.c`) |

- Ergebnis je Uhr: obelix und qemu_emery alle vier; asterix, getafix, flint
  und gabbro nur `BOULDER_SCHLAF` (Schlaf ohne Phasen, Upstream-Health).
  HRV, Messstufe und Health stehen nicht in `prj_normal.conf`: auf Uhren ohne
  Pulssensor gäbe das eine Kconfig-Warnung, und die bricht den Bau ab.
- Pebbles Dateien rufen nur die Haken in
  `include/pbl/services/activity/boulder/haken.h` (`activity.c`,
  `activity_sessions.c`, `activity_metrics.c`, Kraepelin). Ist ein Schalter
  aus, sind sie leer oder sagen „nicht zuständig“, und Pebbles Code läuft wie
  Upstream. Einstellungen und Prefs (`settings/health.c`, `prefs.c`,
  `settings_blob_db.c`) mit `#ifdef`.
- Ohne Schalter, nur bewacht: die erkannte Bewegung (Typ 8) und der
  Gehfilter. Sie bleiben in jedem Bau Boulder.
- Ohne Schalter, weil es Upstream-Fehler sind: `activity_sessions.c` rückt
  beim Entfernen mit `memmove` nach (Quelle und Ziel überlappen) und nullt
  beim Zurücknehmen den frei gewordenen letzten Platz (sonst stand die letzte
  Sitzung nach einem Neustart doppelt da). Beides gilt auch mit
  `BOULDER_SCHLAF=n`, etwa wenn Kraepelin einen Schlafzyklus verwirft.
- Probebau mit einem Schalter aus, **nur mit eigenem Bauordner (`-b`)**:
  `pbl configure -b build-x --board obelix@pvt -DCONFIG_RELEASE=y -DCONFIG_BOULDER_HRV=n`.
  `BOULDER_SCHLAF=n` schaltet alle vier ab. In `build/` bliebe der Schalter
  im CMake-Cache stehen; `bundle.sh` konfiguriert darum mit `--fresh`, bricht
  bei `build/menuconfig.conf` ab und prüft für obelix in `build/.config`,
  dass alle vier `=y` sind (Test `test_boulder_bundle_schalter`).
- Host-Tests in beiden Modi: `test_activity` und `test_activity_upstream`,
  `test_activity_algorithm_kraepelin` und `…_upstream`, `test_boulder_nacht`,
  `…_ohne_hrv`, `…_ohne_messstufe`; Pebbles Health-Tests
  (`tests/fw/apps/system_apps/health/`) neben Boulders
  (`…/boulder_health/`).

## Datalogging-Tags und Verträge

- Die Nacht geht auf **`0x4001`** (`NachtDlsRecord`, 328 Byte), ihre
  HRV-Fenster auf **`0x4002`** (`NachtHrvDlsRecord`, 251 Byte), beide mit
  `UUID_SYSTEM` (`boulder/nacht.h`). Bis b24 waren es 88 und 89 - Pebbles
  nächste freie System-Tags; ein Upstream-Merge hätte sie still doppelt
  belegt. 88 und 89 bleiben gesperrt: eine liegende Sitzung von b24 geht nach
  dem Update mit ihrem alten Tag hinaus. `data_logging_service.h` ist wieder
  Upstream.
- Boulder braucht für `0x4001`/`0x4002` mindestens die App 0.50.0. Eine
  ältere quittiert unbekannte Tags und wirft sie weg; deshalb die Zeile
  „Mindest-App“ im Release. Die wertet aber erst 0.50 aus: App 0.49 bietet
  b25 trotzdem an und verlöre die Nächte. Das Telefon muss also vor dem
  Veröffentlichen von b25 auf 0.50 oder neuer stehen; `release.sh` prüft
  dafür, dass das App-Release veröffentlicht ist (siehe Releases).
- **Wächter** für das, was Boulder mit Upstream teilt (Typ 8, Tag 84 Fassung
  7 mit `manual` an Byte 26, die Tags):
  - `src/fw/services/activity/boulder/vertraege.c`: Static-Asserts und
    `switch` ohne `default` über `ActivitySessionType` und `DlsSystemTag` -
    ein neuer Upstream-Wert bricht den Bau; ein `switch` über alle vergebenen
    Tag-Zahlen bricht bei einem doppelten.
  - `tools/boulder/vertraege.py`: Namen und Werte gegen eine eingefrorene
    Liste, als Bauschritt (`pbl_boulder_vertraege`) und als ctest
    (`test_boulder_vertraege`, Gegenproben in `test_boulder_vertraege_wache`).
    Nur hier fällt ein neuer Name mit einem schon vergebenen Wert auf
    (`Swim = 8` vor `Movement = 8`); der Compiler sieht das nicht.
  - Schlägt ein Wächter nach einem Upstream-Merge an: erst klären, was die
    App dann liest (`HealthDataProcessor`, `Datalogging`), dann Liste,
    `vertraege.c` und diesen Abschnitt nachziehen.

## Einstellungen (shellpref)

`SHELL_PREFS_FILE_LEN` ist 3072 statt 2048 (`prefs_private.h`): alle Prefs
belegten in b24 schon 1990 Byte. 3072 bleibt dieselbe eine PFS-Seite von
4011 Byte, die Datei wird beim Update nicht umgeschrieben, und eine ältere
Firmware verdichtet den Bestand ohne Verlust. `prefs.c` prüft beim Bauen,
dass alle Prefs plus der größte Satz passen und die Grenze höchstens 3343
bleibt (darüber zwei Seiten - dann verlöre ein Rückweg alle Prefs). Test:
`test_shellpref_groesse`.

## Kieselsport in der Firmware (F4)

Der Trainingsdienst `src/fw/services/kieselsport/` ist der Worker der
Store-App Kieselsport, auf KernelBG. Die Logik (Training, Puls und Zonen,
Wiederholungen, Bahnen, Kurve, Archiv) ist aus Kieselsport übernommen; nur
`plattform.c` bildet ab, was dort über das SDK lief. Der Dienst schreibt in
den Persist mit der Kennung von Kieselsport, damit die System-App dieselben
Daten sieht wie vorher App und Worker.

Anders als der Worker:

- eigene Anmeldung am Pulssensor, mit Ablauf nach fünf Minuten ohne
  Erneuerung; Pulsmodell je Sportart wie in der Workout-App
- brummt selbst, statt die App nach vorn zu holen
- keine automatische Bewegungserkennung, solange ein Training läuft
- das fertige Training wird eine selbst gestartete Sitzung im Verlauf der Uhr
  (ohne Pebbles Insights-Meldung, die sich über den eigenen Schirm legte)
- holt die App nach 2, 10 und 30 Minuten, wenn eine Zusammenfassung nicht
  beim Telefon ankam - nur vom Zifferblatt aus

Die Oberfläche ist die System-App `src/fw/apps/system/kieselsport/` mit
**derselben UUID wie die Store-App** (6c386250-…): Kiesel-Helper und die
Konfigseite erreichen sie mit denselben Nachrichten (Schlüssel ab 10000 wie
im SDK), und sie teilt den Persist mit dem Dienst. `pebble_compat.h` bildet
die SDK-Namen auf die Firmware ab (Muster der Wetter-App). Die Nacht ist
nicht mehr Teil der App; das macht die Firmware (F2).

- Startmenü: eigene Kachel (`launcher/default/app_glance_kieselsport.c`),
  weil der Launcher Glance-Scheiben nur von Store-Apps nimmt; während eines
  Trainings steht Kieselsport oben.
- Die Workout-App ist abgeschaltet (`"ifdefs": ["DISABLED"]`).
- Eine Store-App mit der UUID einer System-App erscheint nicht doppelt im
  Startmenü, und ihr Entfernen löscht weder Persist noch Pins
  (`app_install_manager.c`). Die Store-App Kieselsport muss von der Uhr.

Firmware-Etappe nach dem App-Audit (neue Schlüssel hinter denen der Store-App,
die bis 10036 gingen; ein Telefon, das sie nicht kennt, überliest sie):

- `ZEIT` 10037: jede Zustandsmeldung (Start, Pause, Weiter, Stopp) trägt ihre
  Uhrzeit. Die Meldungen stehen in einer Schlange (`meldungen.c`, acht
  Plätze, je sechs Anläufe), statt einander zu ersetzen; die Zusammenfassung
  wartet, bis die ihres Trainings durch sind.
- `PAUSEN` 10038 (Byte: Fassung 1, Zahl aller Pausen, je Pause aktive Sekunde
  und Dauer als uint16 LE, höchstens 32) und `ENDE` 10039 (Beginn der letzten
  Pause, UTC) in der Zusammenfassung. Persist 33/34 neben der wartenden; 34
  trägt mit dem Ende den Beginn des Trainings, sonst gelten beide nicht
  (ältere Firmware löscht sie nicht mit).
- Der Stopp eines verworfenen Trainings liegt im Persist (35), bis das Telefon
  ihn bestätigt; das Nachsenden holt die App auch dafür, und die App fasst
  nach, bis er drüben ist. Ein besetzter Postausgang zählt nicht als Anlauf.
- Maximalpuls „nicht gesetzt“: ohne Einstellung Tanaka aus dem Alter der Uhr
  (ohne Abgleich 30, also 187). `MAXPULS_GESETZT` 10040 in beide Richtungen;
  Persist 12 vermerkt, ob gesetzt. Eine alte Zahl ohne Vermerk heißt „nicht
  gesetzt“, wenn sie 190 (alte Vorgabe) oder Tanaka nach dem Alter der Uhr
  ist (Boulder 0.43–0.45 schickte bei „From age“ die Schätzung); sonst
  eingestellt. Entschieden wird beim ersten Gebrauch, nicht beim Start (dann
  ist das Alter noch nicht geladen), und das Ergebnis kommt in den Persist.
  Vom Telefon gilt eine Zahl ohne Vermerk als eingestellt, außer sie ist das
  Echo des Werts nach Alter.
- `KCAL`, `PULS_MITTEL`, `PULS_MAX` gehen weiter mit; Boulder braucht sie nicht
  mehr (KCAL nur, bis die Kalorien aus dem Puls da sind).

## Kieselstrasse in der Firmware (Navigation)

Die System-App `src/fw/apps/system/kieselstrasse/` zeigt den nächsten Schritt
einer OsmAnd-Navigation. Sie trägt **dieselbe UUID wie die Store-App**
(a83bf269-e798-45bc-9bf0-a0f68362a79a) und empfängt dieselben Nachrichten von
Boulder mit festen Schlüsseln (`schluessel.h`): ABBIEGEART=10000,
ENTFERNUNG=10001, STRASSE=10002, ANKUNFT=10003, REST=10004.

- Auf Abruf: Boulder verbindet sich nur mit OsmAnd, solange die App auf der
  Uhr offen ist, und holt sie nicht mehr nach vorn. Keine eigene Kachel, kein
  Glance (der Launcher nimmt von System-Apps ohnehin keine Scheiben).
- Persist-Schlüssel 1, Struktur `Weg` unverändert (68 Byte): der letzte Stand
  der Store-App bleibt lesbar. Geschrieben wird nur bei einem neuen Schritt
  und beim Beenden, wenn seither etwas kam.
- Aller statische Zustand wird bei jedem Start zurückgesetzt (System-Apps
  behalten ihre statischen Variablen über ihr Ende hinaus).
- Texte in fünf Sprachen (`texte_tabelle.h`), eigene Namen (`ST()`,
  `KST_`), damit nichts mit Kieselsport kollidiert.
- Kein eigener Kconfig-Schalter: die App braucht keinen Dienst und keine
  Hardware und ist klein; Kieselsport ist in `prj_normal.conf` ohnehin für
  alle Uhren an.
- Test: `test_kieselstrasse_weg` (Schritt-Erkennung, sparsames Schreiben,
  alter oder kaputter Persist). Prüfbau fürs Emulator-Bild: `-DKST_DEMO`.

## TimeStyle Bedrock (System-Zifferblatt)

`src/fw/apps/watch/timestyle/` ist [TimeStyle](https://github.com/freakified/TimeStylePebble)
(MIT, Stand e2be725) mit pebble-fctx 1.6.5 und pebble-utf8 1.0.4 (beide MIT,
JR Mobley); die Lizenzen liegen neben dem Code, jede Änderung am übernommenen
Code ist mit `Boulder:` markiert. Grund: die Store-Fassung hält über PebbleKit
JS dauernd eine WebView auf dem Telefon, solange sie Zifferblatt ist.

- **Kein PebbleKit JS, keine Konfigseite auf der Uhr.** Das Wetter kommt aus dem
  Wetterspeicher der Firmware, den Boulder ohnehin füllt
  (`wetter_quelle.c`: `weather_service_create_default_forecast`, dazu aus
  dem v4-Satz Ort, Tageswetter und UV; ab b24 der UV-Wert der Stunde jetzt aus
  `today_hourly_uv_x10` in der Zeit des Wetterorts, wie das Original - nach
  Mitternacht am Ort „…“, bis Boulder neue Daten schickt; ohne Stundenwerte der
  Tageswert, `wetter_uv_jetzt`). Neu gelesen beim Start, bei
  `PEBBLE_WEATHER_EVENT` und alle 15 Minuten. Älter als 3 Stunden oder
  fehlend: „…“ ohne Symbol, wie TimeStyle ohne Wetter. Temperaturen in der
  Einheit, die in Boulder für das Wetter gewählt ist (die Uhr rechnet nicht
  um; eine eigene °C/°F-Wahl des Zifferblatts gibt es bewusst nicht). Sonne und „teils bewölkt“ nachts mit Mond (Sonnenstand
  aus dem Ort des Satzes); ein Gewitter kennt die Firmware nicht.
- **Einstellungen aus Boulder** (ab b23, Boulder 0.48): ein Wert `tsBedrock`
  von 32 Byte mit Fassungsbyte in der Uhreinstellungen-Synchronisation
  (Whitelist in `settings_blob_db.c`, Wert in `prefs.c`, Aufbau und
  Leseregeln in `ts_format.c`). Nur Telefon → Uhr; die Uhr schreibt ihn nie.
  Ein falsch langer Wert wird abgewiesen (der vorige bleibt). Das laufende
  Zifferblatt hört auf `PEBBLE_PREF_CHANGE_EVENT` und zeichnet sofort neu.
  Fehlt der Wert (alter Boulder) oder ist ein Feld ungültig, gelten die
  Vorgaben vom Foto des Nutzers vom 03.10.2026: schwarz, weiße LECO-Ziffern,
  weiße Leiste rechts mit schwarzer Schrift, Wetter / Schritte / Datum,
  Englisch. 24 h und km/mi folgen der Uhr. Ab b24 liest die Uhr auch Byte 6
  (Schrift 0–4 wie im Original: Regular, LECO, Bold, Bold Hour, Bold Minute;
  Vorgabe bleibt LECO) und alle 37 Sprachen; GK, VN, RU, CN, UK, JP, KR und
  IW brauchen wie im Original ein passendes Sprachpaket (die Systemschriften
  holen die Zeichen aus dessen Erweiterung), ohne Paket zeigt die Uhr
  Ersatzkästchen. Nicht einstellbar: °C/°F (folgt Boulder), der Wetterort
  (immer Ort 0 aus Boulders Liste, „automatisch“).
- Widget „Time zone“: die Stunde ohne doppelt abgezogene Sommerzeit und auf
  die Minute genau (`zweitzeit.c`); km-Distanz mit dem gewählten
  Dezimaltrenner.
- Name „TimeStyle Bedrock“, eigene Kennung d8af11d3-1e92-4f6d-b494-c5572f7bb7df (in b21 noch „TimeStyle“ mit e5c8eb9b-…, wird einmal umgestellt), Registry -104, als
  erstes Zifferblatt der Liste (der Rückfall auf „das erste Zifferblatt“
  trifft TimeStyle). TimeStyle aus dem Store bleibt daneben installierbar.
- **Standard:** `prefs.c` stellt beim ersten Abfragen des Zifferblatts
  EINMAL auf TimeStyle um, wenn das gespeicherte fehlt, TicToc oder die
  Store-TimeStyle (4368ffa4-…) ist; Merker `boulderTimeStyle` in den
  Einstellungen (nicht synchronisiert). Eine spätere eigene Wahl bleibt.
- Nur emery (`CONFIG_APPS_WATCH_TIMESTYLE`, hängt an `PLATFORM_EMERY`,
  `SERVICE_WEATHER`, `SERVICE_ACTIVITY`); die Ressourcen (`TIMESTYLE_*`,
  `resources/normal/base/timestyle/`) tragen `"config"` und fallen sonst weg.
  Herkunft und MIT-Lizenz der Ressourcen: `resources/normal/base/timestyle/LICENSE`.
- **Schriften:** statt Avenir Next (kommerziell, nicht übernommen) Kumbh Sans
  (SIL OFL 1.1) bei Stärke 350 („Regular“) und 600 („Bold“), auf die
  Ziffernhöhe, Oberlänge und tabellarische Ziffernbreite der Avenir-Fassung
  gebracht, damit die Uhrzeit wie im Original sitzt. Bau:
  `tools/boulder/timestyle_schriften.sh` (SVG-Schrift aus
  `tools/boulder/ttf_zu_svgfont.py`, dann pebble-fctx-compiler 1.2.2 wie im
  Original). Gewählt aus 19 freien Schriften (alle aus github.com/google/fonts)
  nach der Flächenüberdeckung mit den Avenir-Ziffern des Originals, je bei der
  besten Stärke (`tools/boulder/timestyle_schriftvergleich.py`, Liste dort):
  Regular Kumbh Sans 0.760, Open Sans 0.736, Hanken Grotesk 0.711, Lato 0.689,
  Plus Jakarta Sans 0.672 … Figtree 0.601, Nunito Sans 0.596, Mulish 0.584.
  Für „Bold“ allein lägen Hanken Grotesk (0.833) und Open Sans (0.821) vor
  Kumbh Sans (0.794); genommen ist trotzdem EINE Familie, weil „Bold Hour“ und
  „Bold Minute“ beide Stärken nebeneinander zeigen, und Kumbh Sans hat bei
  Regular (der Vorgabe) die beste Deckung und wie Avenir 6 und 9 mit geradem
  Strich (Deckung der 6: 0.72, Open Sans 0.48). Mit Glättung
  (alle außer LECO) belegt der Merkerpuffer 200 × 228 Byte im App-Heap; im
  Emulator blieben dabei mindestens 77 400 Byte frei (mit LECO 116 500;
  gemessen 4.10.2026 je Schrift und mit großer Leiste, Messbau nicht im Repo).
- Nicht übernommen: `messaging.c`, die Aplite-Teile, `fpath`.
- `pebble.h` im Ordner steht für das SDK-`<pebble.h>`; Bilder werden als
  beschreibbare Kopie geladen (`ts_pdc_laden`), weil System-Ressourcen im
  Flash liegen können und TimeStyle sie umfärbt.
- Tests: `test_timestyle` (Wetterart → Symbol, Tag/Nacht auf die Minute und
  als Fingerabdruck – dieselben Zahlen prüft Boulders Vorschau –, Alter,
  „5.5k“, Distanztexte an den Grenzen, UV der Stunde),
  `test_timestyle_wetter_quelle` (der Lesepfad `wetter_quelle.c` mit
  nachgebauter Datenbank: Ort 0, Zeitzone ab v4.1, UV je Stunde ab v4.4 samt
  Längenprüfung, was an `wetter_uv_jetzt` geht),
  `test_timestyle_standard` (die einmalige Umstellung in `prefs.c`; eigener
  Test, weil `test_prefs_db` ohne `CONFIG_APPS_WATCH_TIMESTYLE` baut),
  `test_timestyle_einstellungen` (Leseregeln mit den gemeinsamen
  Testvektoren V0–V8 wie in Boulders `TsBedrockFormatTest`, je Schalter-Bit
  ein eigener Fall, Schriften samt Zuordnung je Zeile aus `schriftwahl.c`,
  Sprachen, Zeitzone),
  `test_timestyle_bedrock_pref` (Whitelist, Länge auf beiden Einfügewegen,
  Ereignis). Boulders Vorschau ist ein Nachbau dieses Zifferblatts und wird
  gegen Emulatorbilder des Prüfbaus auf das Pixel geprüft
  (`TsZifferblattReferenzTest`, Bilder und `erzeugen.sh` in Boulder unter
  `pebble/src/androidHostTest/resources/timestyle/referenzen/`).
  Prüfbau fürs Emulator-Bild mit festen Werten (Sa 3.10.2026 15:45, Wetter
  22/24/12, UV 3, 5523 Schritte, 4230 m, Puls 62, Schlaf 7h 23m bzw. 2h 5m,
  Akku 80 %): `-DTS_DEMO` (`pbl configure … -DCMAKE_C_FLAGS=-DTS_DEMO`); ein
  Bau mit `CONFIG_RELEASE` und `TS_DEMO` bricht ab (`pebble.h`); das echte
  Wetter im Emulator: `pbl feed weather`; Einstellungen wie aus Boulder
  (mit Zeitstempel, `--plain-insert` für den alten Weg):
  `pbl feed timestyle --left --large --font 2 …` oder `--hex <64 Zeichen>`.

## Releases (F5)

```
tools/boulder/release.sh <N>       # Tag v4.38.4-bN, beide Slots, Release in boulder-firmware
```

Die Pakete liegen im öffentlichen Repo
[dysseus-pascal/boulder-firmware](https://github.com/dysseus-pascal/boulder-firmware),
weil dieser Fork privat ist und Boulder ohne Anmeldung nach Updates fragt.
Jedes Release trägt eine Ed25519-Signatur (`<paket>.sig`, siehe
`tools/boulder/SIGNATUR.md`); ohne den privaten Schlüssel bricht `release.sh`
ab, und Boulder nimmt nichts Unsigniertes an.
Vorher `.github/release/v4.38.4-bN.md` anlegen (erste Zeile Titel ohne
Version). Boulder vergleicht erst die PebbleOS-Fassung, dann die Nummer nach
dem `b`, und bietet eine neue Fassung sofort an. Die Nummer zählt über die
Basis hinweg weiter (v4.38.1-b6, dann v4.38.3-b7).

**Mindest-App:** die Notizen tragen unter dem Titel genau einmal eine eigene
Zeile `Mindest-App: X.Y.Z` (ohne Leerzeichen davor oder dahinter, LF; CR lehnt
`mindest_app.py` ab, obwohl die App sie nähme). Boulder ab 0.50 liest sie mit
`^Mindest-App: (\d+\.\d+\.\d+)$` aus dem Text des Releases und bietet die
Firmware einer älteren App nicht an; fehlt die Zeile, gilt keine Grenze. **App
0.49 und älter werten die Zeile nicht aus** und bieten jede Firmware an: vor
dem Release muss das Telefon die Mindest-App schon haben. `release.sh` bricht
vor dem Tag ab, wenn die Zeile fehlt, doppelt, falsch geschrieben oder älter
als `tools/boulder/mindest_app.txt` ist (Prüfung: `tools/boulder/mindest_app.py`)
oder wenn das App-Release `v<Mindest-App>` in `dysseus-pascal/Boulder` fehlt
oder ein Entwurf ist (`gh release view`); Test
`test_boulder_release_mindest_app`. Wer etwas einbaut, das eine neuere App
braucht, zieht `mindest_app.txt` nach; seit den Tags `0x4001`/`0x4002` steht
dort 0.50.0.

**Vorabversion:** `BOULDER_VORAB=1 tools/boulder/release.sh <N>` macht aus dem
Entwurf direkt ein Pre-release, ohne Zwischenstand als normales Release.
Boulder überspringt Pre-releases (`GithubReleases.kt`) und bietet sie nicht an;
der Nutzer lädt die `.pbz` und öffnet sie in Boulder (mit Rückfrage; dabei
prüft Boulder 0.50 nur die Prüfsumme, nicht die Signatur). Nach dem Test auf
der Uhr freigeben mit
`gh release edit v4.38.4-bN -R dysseus-pascal/boulder-firmware --prerelease=false --latest`;
erst dann bietet Boulder sie an.

Upstream-Fassungen kommen per Merge des Tags. Nicht übernommen: `b97d3837e`
(SpO2 aus dem Minutensatz, Version 14 → 13); Boulder liest Version 14. Die geerbten GitHub-Actions dieses Forks sind abgeschaltet;
gebaut wird lokal.

## Health-App (F4)

`src/fw/apps/system/boulder_health/` ersetzt auf emery Pebbles Health-App
unter derselben Kennung (Zifferblatt-Taste und Übergang bleiben). Gebaut wird
genau eine: `apps/system/CMakeLists.txt` nimmt mit `BOULDER_HEALTH_APP`
Boulders, sonst Pebbles aus `src/fw/apps/system/health/` (Stand der
Merge-Basis 49c24cfd8, unverändert). Beide exportieren `health_app_get_info`;
Registry, Quick-Launch und Insights-Pins bleiben. Auf flint, gabbro, asterix
und getafix läuft also Pebbles Health-App. Drei Karten, Oben/Unten
blättert: **Heute** (Schritte, km, kcal, aktive Minuten, Schritte je Stunde),
**Schlaf** (letzte Nacht aus `boulder_nacht_letzte`: Dauer, Zeiten,
Hypnogramm, Minuten je Phase), **Herz & SpO2** (Puls live mit schlagendem
Herz, Ruhepuls und HRV der Nacht, letzter SpO2, Puls der letzten 24 h,
tiefster SpO2 der Nacht). Nur auf der Herz-Karte misst der Sensor
sekündlich. SpO2 steht auf „off“, wenn die Uhr nicht misst: Schalter
„Blood Oxygen“ aus, Messstufe 1 oder, seit b25, Pebbles Auswahl „Disabled“ -
eine Angleichung an `activity.c`, das dann auch nicht misst; bis b24 zeigte
der Ring dort „an“ (Test `test_health_daten`). Die Minutenhistorie für die Kurven liest ein Timer stückweise,
damit die App sofort erscheint.

Dafür merkt sich die Firmware die letzte Nacht (auch über einen Neustart, Datei
`boulder_nacht`) und die SpO2-Werte im Ring (`boulder/spo2.c`, 96 Werte).

Emulator: `pbl configure -b build-qemu --board qemu_emery && pbl build -b build-qemu && pbl qemu -b build-qemu`.
