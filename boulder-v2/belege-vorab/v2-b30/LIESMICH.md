# V2 v4.41.1-b30: Health blättert mit Unten vorwärts (14.10.2026)

b29 mit F23 (`../../entscheidungen.md`; Bericht `../../schritt3-abnahme.md`, Abschnitt 14): In der
Health-App blättert Unten zur nächsten Karte, Oben zurück, nach jedem Start.

## Paket

| | |
|---|---|
| Datei | `normal_obelix_pvt_v4.41.1-b30.pbz`, 3 449 406 Byte |
| sha256 | `c64063ce3c352023966d6b01e6bb21ae0fa56446aa44506c52246848f378dde4` |
| Stand | V2 `a3652f881` (Zweig `v2`), annotiertes Tag `v4.41.1-b30` (nur lokal) |
| Inhalt | `slot0/` und `slot1/` wie bei b29; Manifest `versionTag` v4.41.1-b30, `hwrev` obelix_pvt; Abbilder verschieden gelinkt, Kieselstrasse in beiden, `WorkerState` 640 Byte |
| Bau | `skripte/bundle_b30.sh`, beide Slots ohne Warnung (`bau.txt`) |

**Nicht signiert und nicht veröffentlicht**, wie b27 bis b29. Der Nutzer öffnet die `.pbz` in Boulder.

## Prüfung

| Datei | Inhalt |
|---|---|
| `pruefung.txt` | auf `a3652f881`: acht Bauten ohne Warnung, Host 440/440, alle Werkzeuge grün (im Prüfbau nur die Fehlerprobe rot, wie vorgesehen) |
| `host.txt` | der volle Host-Lauf |
| `qemu-a/`, `qemu-b/` | zwei QEMU-Läufe, alle Prüfungen grün: L1 höchstens 12,7 bzw. 17,6 ms, erster Start (icount) 19,5 bzw. 20,5 ms, L2 23,4 bzw. 11,7 ms, L4 10,7 bzw. 9,8 ms, L6 ≤ 0,02 ms; Kernel-Heap ohne Wachstum, kein Neustart. Das Skript blättert jetzt mit Unten |
| `karten-reihenfolge.png` | die Karten aus `qemu-a/bilder`, mit Unten aufgenommen: Today, Night, Heart, Activity, Settings |
| `skripte/` | Prüflauf, QEMU, Bau des Pakets |

Mutationen K5a (Oben wieder vorwärts) und K5b: rot (`../../belege-schritt3/ap9/mutationen.py`).
