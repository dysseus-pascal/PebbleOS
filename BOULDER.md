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

Emulator: `pbl configure -b build-qemu --board qemu_emery && pbl build -b build-qemu && pbl qemu -b build-qemu`.
