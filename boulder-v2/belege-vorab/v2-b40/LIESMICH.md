# V2 v4.42.0-b40: Schlafmodus von Hand (27.10.2026)

b39 mit dem Schlafmodus von Hand wie Morpheuz, F53–F56 (`../../entscheidungen.md`, Abschnitt
„Schlafmodus von Hand wie Morpheuz“). Der Nutzer: „manual mode hast du richtig verstanden. morpheus
wurde erwähnt“. Dazu gehört Boulder 0.61.0.

- **Kern** (V2 `20aa3bb86`): Mit dem Start beginnt das Nachtfenster in dieser Minute statt um 20 Uhr,
  auch am Tag; `prv_fenster` bleibt die einzige Stelle, die es bestimmt. Der Start schliesst das
  laufende Fenster gleich, „aufgewacht“ macht die Nacht fertig und endgültig. Sonst endet der
  Schlafmodus mit der endgültigen Nacht oder nach zwei Stunden ohne Schlaf. Bit 0x02 im Nachtsatz,
  Zustand in `boulder_v2` unter `hand`, Nachfüllen nach einem Neustart, ein Wunsch vor „Laden“ gilt
  ab dem Laden. Neu in `schlafmodus.c`; in Boulders Dateien die Haken in `dauerteil.c`, `nacht.c`,
  `ausgabe.c` und `speicher.c`. Keine Pebble-Datei.
- **Health-App und Schalter** (V2 `3ed44d560`): Select lange auf Night startet; im Schlafmodus öffnet
  die App auf Night („Sleeping since“), Select beendet ihn. Beides über S8, der Pebbles Schalter
  öffnet. Der Schalter „Toggle Sleep Mode“ (`fw/apps/system/boulder_schlaf/`, Registry -105) ist wie
  Pebbles Schalter für Quiet Time gebaut. Einzige Pebble-Datei: der Eintrag in
  `system_app_registry_list.json`, unter dem Schalter der Variante (`k1_waechter.py` grün).

## Paket

| | |
|---|---|
| Datei | `normal_obelix_pvt_v4.42.0-b40.pbz`, 3 506 893 Byte |
| sha256 | `da48e5077f1b06e5784212cfbaab08ae4954941b9aa18ae91ddd244c121859f5` |
| Stand | V2 `3ed44d560` (Zweig `v2`); Tag `v4.42.0-b40` nur in einem Wegwerf-Klon |
| Inhalt | `slot0/` und `slot1/` wie bei b39; Manifest `versionTag` v4.42.0-b40, `commit` 3ed44d560, `hwrev` obelix_pvt (`manifest.txt`) |
| Bau | `skripte/bundle_b40.sh`, beide Slots ohne Warnung (`bau.txt`) |
| Release | v4.42.0-b40 in boulder-firmware (27.10.), Lauf 40511275403, Tag auf `a1d325c`; Signatur mit `signatur.pub` geprüft, Gegenprobe rot |

## Prüfung

| Datei | Inhalt |
|---|---|
| `pruefung.txt` | auf `3ed44d560`: acht Bauten ohne Warnung, alle Werkzeuge grün (im Prüfbau nur die Fehlerprobe rot, wie vorgesehen), QEMU-Abbild |
| `host.txt` | Host-Tests auf `3ed44d560`: 474/474 (neu `test_boulder_schlafschalter`) |
| `mutationen.txt` | 32 Mutationen (`skripte/mutationen.py`) auf dem Arbeitsstand vor den Commits: Fenster, Ende, Bit, Ablage, Laden, S8, Health-App und Schalter. Im ersten Lauf 30 rot; D4 (Start schliesst das alte Fenster nicht) und N1 („aufgewacht“ macht die Nacht nicht fertig) überlebten. Mit den Tests `schlafmodus_erste_minute` und `schlafmodus_aufgewacht_sofort` sind beide rot (Nachtrag am Ende), also 32/32 |
| `bilder/` | QEMU (`skripte/schlaf_bilder.py`, `skripte/quick_launch_bild.py`): Today, Night, Select lange („Sleep Mode Started“), das Zifferblatt danach, die Health-App im Schlafmodus („Sleeping since 8:01 PM“), Select („Sleep Mode Ended“), der Schalter aus Quick Launch an und aus, und Pebbles Quick-Launch-Auswahl mit „Toggle Sleep Mode“ neben „Toggle Quiet Time“ |
| `pruef/` | Logs der Schlussprüfung |

- **Neue Tests:** in `test_boulder_zyklen` die Nacht von Hand am Tag, am Abend, mit Neustart im
  Schlaf, ohne Schlaf, gleich beendet, nach 15 Stunden, vor dem Laden, die Nachtruhe der Messung,
  die erste Minute nach dem Start und „aufgewacht“ gleich beim Aufwachen; S8 und die Lage in
  `test_boulder_schnittstelle`; die Tasten in `test_bh_app`; Night im Schlafmodus mit 24 h und
  12 h als Bilder in `test_bh_karten`; der Schalter in `test_boulder_schlafschalter`.
- **Im Emulator** hält QEMUs `sendkey` eine Taste nicht: „Select lange“ kommt dort mit Pebbles
  Befehl `button hold 2 1000` (`remote_input_shell.c`).
