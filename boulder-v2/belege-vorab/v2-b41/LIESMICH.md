# V2 v4.42.0-b41: Zifferblatt Tagesband (27.10.2026)

b40 mit dem Zifferblatt Tagesband, F57 (`../../entscheidungen.md`, Abschnitt „Zifferblatt
Tagesband“). Der Nutzer: „mach mir 3 vorschläge für ein digitales watchface das thematisch zu bolder
passt“, „mach mokups“, „nimm 1, das Tagesband“. Keine neue App nötig, Mindest-App bleibt 0.61.0.

- **Zifferblatt** (V2 `7d5d983b1`, `fw/apps/system/boulder_tagesband/`): Zeit gross wie bei Kickstart,
  Datum, das Band der letzten 18 und der nächsten 6 Stunden (Nacht in Phasen aus S1, Pebbles
  Sitzungen, laufender Schlafmodus, Puls je Halbstunde aus S6), unten Schritte, Ruhepuls und HRV des
  heutigen Wachtags, im Schlafmodus „Sleeping since“. Ohne Systemrechte, über die Datenschicht der
  Health-App. Einzige Pebble-Datei: der Eintrag `BOULDER_TAGESBAND` (-106) in
  `system_app_registry_list.json`, unter `CONFIG_BOULDER_TAGESBAND` (`k1_waechter.py` grün).

## Entwurf

`entwurf/`: die drei Vorschläge (Tagesband, Schichten, Höhenlinien), gezeichnet mit Pebbles Grafik
und Schriften in einem Wegwerf-Test (`mockup_test.c.txt`), dazu das Übersichtsblatt
`zifferblaetter.png`. Gewählt wurde 1.

## Paket

| | |
|---|---|
| Datei | `normal_obelix_pvt_v4.42.0-b41.pbz`, 3 511 426 Byte |
| sha256 | `07b4881ff5156767fb85cc742a05927a5e5eb573fac971f3ecce838747ba12a8` |
| Stand | V2 `7d5d983b1` (Zweig `v2`); Tag `v4.42.0-b41` nur in einem Wegwerf-Klon |
| Inhalt | `slot0/` und `slot1/` wie bei b40; Manifest `versionTag` v4.42.0-b41, `commit` 7d5d983b1, `hwrev` obelix_pvt (`manifest.txt`) |
| Bau | `skripte/bundle_b41.sh`, beide Slots ohne Warnung (`bau.txt`) |
| Release | v4.42.0-b41 in boulder-firmware (27.10.), Lauf 40591888503, Tag auf dem Release-Commit `15c8f28`; Signatur mit `signatur.pub` geprüft (`sig/`), Gegenprobe mit falscher Grösse rot |

## Prüfung

| Datei | Inhalt |
|---|---|
| `pruefung.txt` | acht Bauten ohne Warnung, alle Werkzeuge grün (im Prüfbau nur die Fehlerprobe rot, wie vorgesehen), QEMU-Abbild; gelaufen auf dem Arbeitsstand vor dem Commit |
| `nachher.txt` | auf `7d5d983b1` nach clang-format und den Pulslücken der Prüfdaten: die vier Bauten mit dem Zifferblatt ohne Warnung, `pruefe_ohne_rechte.py` und `k1_waechter.py` grün |
| `host.txt` | Host-Tests auf `7d5d983b1`: 478/478 (neu `test_tagesband_band`, `test_tagesband_bild`, `test_tagesband_app`, `test_tagesband_unbenutzt`) |
| `mutationen*.txt` | 31 Mutationen (`skripte/mutationen*.py`): Band, Puls, Marken, Datum, Zeit, Werte von heute, Bild und Lebenslauf. Im ersten Lauf 25 von Tests rot; fünf scheiterten schon am Bau (`-Werror`: unbenutzte Variable oder Funktion) und sind im Nachtrag baubar neu formuliert, alle rot; G7 (einzelne Halbstunde ohne Punkt) überlebte, bis die Prüfdaten von `test_tagesband_bild` zwei Lücken im Puls bekamen. Also 31/31 |
| `bilder/` | QEMU (`skripte/qemu_*.py`, Prüfbau `build-pruef`): das Zifferblatt mit einer Prüfnacht (Ruhepuls 52, HRV 56), Pebbles Liste der Zifferblätter mit „Tagesband“, die Wahl als Zifferblatt, ohne Nacht, im Schlafmodus („Sleeping since 8:01 PM“), eine Minute später und wieder aus. Die Farben in QEMU-Bildern sind blasser als auf der Uhr |
| `pruef/` | Logs der Werkzeuge |

- **Im Emulator:** Nach einer Prüfnacht im Zeitraffer nahm der Prüfbau den Schlafmodus nicht an;
  ohne Prüfnacht ging er wie bei b40. Die Bilder 04 bis 07 sind darum ohne Prüfnacht.
