# V2 v4.41.0-b27: erste Vorabversion (14.10.2026)

Entscheid des Nutzers vom 14.10.: V2 jetzt, mit der HRV-Regel (F20/F21), Kieselstrasse danach, kein
Vorher-Video (`../../entscheidungen.md`, „Erste Vorabversion V2“).

## Paket

| | |
|---|---|
| Datei | `normal_obelix_pvt_v4.41.0-b27.pbz`, 3 436 975 Byte |
| sha256 | `adbb262b38a8ffe84229fd746597720ead714387a53d11256a2b8aa433155158` |
| Stand | V2 `14e34d2e` (Zweig `v2`), Tag `v4.41.0-b27` |
| Inhalt | `slot0/` und `slot1/`, je `pebbleos.bin`, `system_resources.pbpack`, `layouts.json.auto`, `manifest.json` (`versionTag` v4.41.0-b27, `hwrev` obelix_pvt); die beiden Abbilder sind verschieden gelinkt; Aufbau wie das b26-Paket |
| Bau | `skripte/bundle_b27.sh`: obelix@pvt, `CONFIG_RELEASE=y`, je Slot ein frischer Bauordner, geprüft auf Boulders Variante, Kieselsport und Health-App und ohne Prüflauf; dann `tools/merge_pbz.py`. Beide Slots ohne Warnung (`bau.txt`) |

**Nicht signiert und nicht veröffentlicht.** In dieser Umgebung fehlen der Signaturschlüssel
(`~/.config/boulder-firmware/signatur.pem`) und ein gültiges `gh`-Token. Das Release in
`dysseus-pascal/boulder-firmware` setzt außerdem das App-Release Boulder `v0.53.0` voraus
(Mindest-App). Der Push des Tags in den Fork wurde abgewiesen (HTTP 403); das Tag steht nur lokal,
der Commit liegt auf `origin/v2`. Der Nutzer bekommt die `.pbz` direkt und öffnet sie in Boulder (der
Weg der Vorabversion; Boulder prüft dabei nur die Prüfsumme).

Für das Release fehlt V2 noch das Werkzeug von b26 (`tools/boulder/release.sh`, `bundle.sh`,
`mindest_app.py`, `signatur.pub`, ihre Tests). Die Notiz dafür steht in `v4.41.0-b27.md`.

## Prüfung

| Datei | Inhalt |
|---|---|
| `pruefung.txt` | acht Bauten ohne Warnung, Host 437/437, alle Werkzeuge grün (im Prüfbau nur die Fehlerprobe rot, wie vorgesehen) |
| `host.txt` | der volle Host-Lauf |
| `qemu-a/`, `qemu-b/` | zwei QEMU-Läufe auf `14e34d2e`, alle Prüfungen grün: L1 höchstens 14,6 bzw. 17,6 ms, erster Start (icount) 20,5 ms, L2 19,5 bzw. 18,6 ms, L4 8,8 bzw. 7,8 ms, L6 0,02 ms; Kernel-Heap ohne Wachstum, kein Neustart |
| `skripte/` | Bau, Prüflauf, QEMU |

Dazu die Mutationen H4a–H4c (rot) in `../../belege-schritt3/ap9/mutationen.py`.
