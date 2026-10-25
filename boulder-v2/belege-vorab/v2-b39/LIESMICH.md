# V2 v4.42.0-b39: PebbleOS 4.42 und nah an der Vorlage (25.10.2026)

b38 mit Pebbles v4.42.0 und `main` bis `f7970c345` und F52 (`../../entscheidungen.md`, Abschnitt
„PebbleOS 4.42 und Core 1.16, nah an der Vorlage“). Der Nutzer: „ja, mach beides. ändere aber
unseren code so das wir immer so nahe an der vorlage wie möglich bleiben.“ Dazu gehört Boulder 0.60.0.

- **Merge** (V2 `09c868b2b`): v4.42.0 und `main`. Pebble speichert die SpO2-Schalter seit v4.42 mit
  dem Takt in einem Satz `spo2Preferences` (3 Byte) und übernimmt beim ersten Start die alten
  Schlüssel. Boulders Stellen rufen Pebbles umbenannte Funktionen; die Zeilentitel heissen wie bei
  Pebble („SpO2“, „SpO2 During Sleep“, „SpO2 During Activity“).
- **Tests** (`12f90a161`, `f3c302d41`): Pebble nennt die Testplattformen nach dem SDK (emery statt
  obelix), die 27 Bilder von `test_bh_karten` heissen darum `~emery.png`; der Startmenü-Test kennt
  die drei neuen Rufe von Pebbles Workout-App.
- **Nah an der Vorlage** (`2823b3ed1`): „Precision“ und „SpO2 During Sleep“ stehen jetzt in
  `fw/apps/system/settings/boulder_health_zeilen.c`. Pebbles `settings/health.c` ruft sie mit vier
  Haken und einem Include, alle unter dem Schalter der Variante: 19 statt 47 Boulder-Zeilen,
  Pebbles Aufzählung und Menüs unverändert. `k1_waechter.py` kennt eigene Dateien jetzt mit ihrem
  Schalter und begrenzt `health.c` auf die Haken (19 Zeilen, 5 Hunks, zwei Funktionen).

## Paket

| | |
|---|---|
| Datei | `normal_obelix_pvt_v4.42.0-b39.pbz`, 3 504 785 Byte |
| sha256 | `bdc8ac380db72f93cccbb219f606a29747ab29bcf8c487ed68fcb074e3d3d1a7` |
| Stand | V2 `2823b3ed1` (Zweig `v2`); Tag `v4.42.0-b39` nur in einem Wegwerf-Klon |
| Inhalt | `slot0/` und `slot1/` wie bei b38; Manifest `versionTag` v4.42.0-b39, `commit` 2823b3ed1, `hwrev` obelix_pvt (`manifest.txt`) |
| Bau | `skripte/bundle_b39.sh`, beide Slots ohne Warnung (`bau.txt`) |
| Release | v4.42.0-b39 in boulder-firmware (25.10.), Lauf 40182768679, Tag auf `81bd589`; Signatur mit `signatur.pub` geprüft (derselbe Schlüssel wie in Boulder 0.60.0), Gegenprobe rot |

## Prüfung

| Datei | Inhalt |
|---|---|
| `pruefung.txt` | acht Bauten ohne Warnung, alle Werkzeuge grün (im Prüfbau nur die Fehlerprobe rot, wie vorgesehen), QEMU-Abbild |
| `host.txt` | Host-Tests auf `2823b3ed1`: 473/473 |
| `mutationen.txt` | 15 Mutationen (`skripte/mutationen.py`): die Haken in `health.c`, Boulders Zeilen und die nach v4.42 angepassten SpO2-Stellen, alle rot; danach alles gebaut und grün |
| `bilder/` | QEMU (`skripte/health_bilder.py`): Einstellungen > Health ohne und mit Tracking, jede Zeile gewählt, SpO2 im Schlaf ein und aus, die Auswahl der Messstufe |
| `pruef/` | Logs der Schlussprüfung |

- **Neuer Test:** `test_boulder_einstellungen__precision` prüft die Zeile „Precision“: Name der
  Stufe, Auswahl ab der geltenden Stufe mit allen fünf, die Wahl gilt; ohne die Variante Pebbles
  „HR Monitoring“. Bis b38 prüfte nur ihr Titel.
- **Gegenproben im Wächter:** die Health-Zeilen unter dem Schalter von Kieselsport, ein Haken ohne
  Schalter und einer in einer anderen Funktion sind rot.
