# V2 v4.41.1-b33: ohne Velo und Auto, nachts alle drei Minuten (20.10.2026)

b32 mit F38 (`../../entscheidungen.md`; Analyse `../../akku-2026-10-19.md`; Bericht
`../../schritt3-abnahme.md`, Abschnitt 17).

- **Velo- und Autoerkennung entfernt** (V2 `4c49e827`): keine Frage nach einer Fahrt mehr und keine
  Vorstufe, die den Pulssensor im Sekundentakt anhielt. Ohne Training zählen bei einer Fahrt wieder
  Schritte, wie vor V2. Für Velofahrten bleibt Kieselsport „Bike“ (Strasse/Gravel, MTB: keine
  Schritte, Puls und Kalorien).
- **Nachtpuls auf „Detailed“ alle drei Minuten** (V2 `0be700c9`), wie b26 es tatsächlich tat; bis b32
  alle zwei.
- **Test** (V2 `9caccff2`): Ein ruhiges Training ist kein Schlaf.

## Paket

| | |
|---|---|
| Datei | `normal_obelix_pvt_v4.41.1-b33.pbz`, 3 441 114 Byte |
| sha256 | `dfd497ed335fd1539058d1d9718b31e9b9319021e48839bcb1ae092604709da8` |
| Stand | V2 `9caccff25` (Zweig `v2`), annotiertes Tag `v4.41.1-b33` (nur lokal) |
| Inhalt | `slot0/` und `slot1/` wie bei b32; Manifest `versionTag` v4.41.1-b33, `hwrev` obelix_pvt; Abbilder verschieden gelinkt |
| Bau | `skripte/bundle_b33.sh`, beide Slots ohne Warnung (`bau.txt`) |

**Nicht signiert und nicht veröffentlicht**, wie b27 bis b32. Der Nutzer öffnet die `.pbz` in Boulder.

## Prüfung

| Datei | Inhalt |
|---|---|
| `pruefung.txt` | auf `9caccff25`: acht Bauten ohne Warnung, Host 440/440, alle Werkzeuge grün (im Prüfbau nur die Fehlerprobe rot, wie vorgesehen und wie bei b32) |
| `host.txt` | der volle Host-Lauf daraus |
| `qemu-a/`, `qemu-b/` | zwei QEMU-Läufe auf `0be700c93` (dieselbe Firmware, der Test ändert sie nicht), alle Prüfungen grün: L1 höchstens 14,6 bzw. 12,7 ms, erster Start (icount) 20,5 ms, L2 11,7 bzw. 13,7 ms, L4 7,8 bzw. 8,8 ms, L6 ≤ 0,02 ms; Kernel-Heap ohne Wachstum (frei 53 708 → 53 632 → 53 632 Byte), kein Neustart |
| `mutationen.txt` | F38a bis F38e (`../../belege-schritt3/ap9/mutationen.py`): alle fünf rot, F38a bis F38c in `test_boulder_training` (F38c im neuen Fall), F38d in `test_boulder_messung`, F38e in `test_boulder_glue` |
| `skripte/` | Prüflauf, QEMU, Bau des Pakets |

- **Kernel-Heap:** Im Release-Bau von obelix hat der Kernel 72 736 Byte Heap, 224 mehr als b32 (weniger
  statischer Speicher). Der Prüfbau legt seit `4c49e827` so viel Ballast an, dass ihm gleich viel bleibt
  (`CONFIG_BOULDER_PRUEFLAUF_HEAP`).
- **Prüfnacht 5 (Velo)** gibt es nicht mehr; `boulder prueflauf` kennt 0 bis 4 (0–3 Fixtures, 4 die
  schlimmste Nacht).
