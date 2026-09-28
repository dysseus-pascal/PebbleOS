# Boulder-Firmware

Fork von PebbleOS (Core Devices, Apache 2.0) ab **v4.38.1**, für die Begleit-App
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
| F3 | Genauere Nacht: Puls jede Minute, HRV, Minutendaten v15 |
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

## Health-App (F4)

`src/fw/apps/system/health/` ersetzt Pebbles Health-App unter derselben
Kennung (Zifferblatt-Taste und Übergang bleiben). Vier Karten, Oben/Unten
blättert: **Heute** (Schritte, km, kcal, aktive Minuten, Schritte je Stunde),
**Schlaf** (letzte Nacht aus `boulder_nacht_letzte`: Dauer, Zeiten,
Hypnogramm, Minuten je Phase), **Herz & SpO2** (Puls live, Ruhepuls und HRV
der Nacht, letzter SpO2, Puls der letzten 24 h, tiefster SpO2 der Nacht),
**Woche** (Schritte und Schlaf der letzten sieben Tage mit Schnitt).

Dafür merkt sich die Firmware die letzte Nacht (auch über einen Neustart, Datei
`boulder_nacht`) und die SpO2-Werte im Ring (`boulder/spo2.c`, 96 Werte).

Emulator: `pbl configure -b build-qemu --board qemu_emery && pbl build -b build-qemu && pbl qemu -b build-qemu`.
