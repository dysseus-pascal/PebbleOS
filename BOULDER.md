# Boulder-Firmware

Fork von PebbleOS (Core Devices, Apache 2.0) ab **v4.38.1**, nachgezogen auf
**v4.38.3**, für die Begleit-App
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
  dem v4-Satz Ort, Tageswetter und UV). Neu gelesen beim Start, bei
  `PEBBLE_WEATHER_EVENT` und alle 15 Minuten. Älter als 3 Stunden oder
  fehlend: „…“ ohne Symbol, wie TimeStyle ohne Wetter. Temperaturen in der
  Einheit des Telefons. Sonne und „teils bewölkt“ nachts mit Mond (Sonnenstand
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
  Englisch. 24 h und km/mi folgen der Uhr. Nicht einstellbar: die Schrift
  (nur LECO), °C/°F (kommt aus Boulder), Sprachen ohne Glyphen in den
  eingebauten Schriften (GK, VN, RU, CN, UK, JP, KR, IW).
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
- Nicht übernommen: die Avenir-Next-Schriften (kommerzielle Schrift; es gibt
  nur LECO), `messaging.c`, die Aplite-Teile, `fpath`.
- `pebble.h` im Ordner steht für das SDK-`<pebble.h>`; Bilder werden als
  beschreibbare Kopie geladen (`ts_pdc_laden`), weil System-Ressourcen im
  Flash liegen können und TimeStyle sie umfärbt.
- Tests: `test_timestyle` (Wetterart → Symbol, Tag/Nacht, Alter, „5.5k“),
  `test_timestyle_standard` (die einmalige Umstellung in `prefs.c`; eigener
  Test, weil `test_prefs_db` ohne `CONFIG_APPS_WATCH_TIMESTYLE` baut),
  `test_timestyle_einstellungen` (Leseregeln mit den gemeinsamen
  Testvektoren V0–V6 wie in Boulders `TsBedrockFormatTest`, Sprachen,
  Zeitzone), `test_timestyle_bedrock_pref` (Whitelist, Länge auf beiden
  Einfügewegen, Ereignis).
  Prüfbau fürs Emulator-Bild mit den Werten des Fotos: `-DTS_DEMO`
  (`pbl configure … -DCMAKE_C_FLAGS=-DTS_DEMO`); das echte
  Wetter im Emulator: `pbl feed weather`; Einstellungen wie aus Boulder
  (mit Zeitstempel, `--plain-insert` für den alten Weg):
  `pbl feed timestyle --left --large …` oder `--hex <64 Zeichen>`.

## Releases (F5)

```
tools/boulder/release.sh <N>       # Tag v4.38.3-bN, beide Slots, Release in boulder-firmware
```

Die Pakete liegen im öffentlichen Repo
[dysseus-pascal/boulder-firmware](https://github.com/dysseus-pascal/boulder-firmware),
weil dieser Fork privat ist und Boulder ohne Anmeldung nach Updates fragt.
Jedes Release trägt eine Ed25519-Signatur (`<paket>.sig`, siehe
`tools/boulder/SIGNATUR.md`); ohne den privaten Schlüssel bricht `release.sh`
ab, und Boulder nimmt nichts Unsigniertes an.
Vorher `.github/release/v4.38.3-bN.md` anlegen (erste Zeile Titel ohne
Version). Boulder vergleicht erst die PebbleOS-Fassung, dann die Nummer nach
dem `b`, und bietet eine neue Fassung sofort an. Die Nummer zählt über die
Basis hinweg weiter (v4.38.1-b6, dann v4.38.3-b7).

Upstream-Fassungen kommen per Merge des Tags. Nicht übernommen: `b97d3837e`
(SpO2 aus dem Minutensatz, Version 14 → 13); Boulder liest Version 14. Die geerbten GitHub-Actions dieses Forks sind abgeschaltet;
gebaut wird lokal.

## Health-App (F4)

`src/fw/apps/system/health/` ersetzt Pebbles Health-App unter derselben
Kennung (Zifferblatt-Taste und Übergang bleiben). Drei Karten, Oben/Unten
blättert: **Heute** (Schritte, km, kcal, aktive Minuten, Schritte je Stunde),
**Schlaf** (letzte Nacht aus `boulder_nacht_letzte`: Dauer, Zeiten,
Hypnogramm, Minuten je Phase), **Herz & SpO2** (Puls live mit schlagendem
Herz, Ruhepuls und HRV der Nacht, letzter SpO2, Puls der letzten 24 h,
tiefster SpO2 der Nacht). Nur auf der Herz-Karte misst der Sensor
sekündlich. Die Minutenhistorie für die Kurven liest ein Timer stückweise,
damit die App sofort erscheint.

Dafür merkt sich die Firmware die letzte Nacht (auch über einen Neustart, Datei
`boulder_nacht`) und die SpO2-Werte im Ring (`boulder/spo2.c`, 96 Werte).

Emulator: `pbl configure -b build-qemu --board qemu_emery && pbl build -b build-qemu && pbl qemu -b build-qemu`.
