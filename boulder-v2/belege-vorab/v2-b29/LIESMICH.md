# V2 v4.41.1-b29: Upstream v4.41.1 für den Gerätetest (14.10.2026)

b28 auf Upstreams Patch-Fassung v4.41.1 (F22, `../../entscheidungen.md`; Bericht
`../../schritt3-abnahme.md`, Abschnitt 13). v4.41.1 korrigiert die Worker von Apps: In v4.41.0 und
damit in b27/b28 startete kein Worker (`worker.txt`). Mit b29 läuft der Gerätetest.

## Paket

| | |
|---|---|
| Datei | `normal_obelix_pvt_v4.41.1-b29.pbz`, 3 449 399 Byte |
| sha256 | `f1e053e30445de0058ac434e68012cec0239c0bd10fbe15d295f7c859438bd31` |
| Stand | V2 `8075766d8` (Zweig `v2`), annotiertes Tag `v4.41.1-b29` (nur lokal) |
| Inhalt | `slot0/` und `slot1/` wie bei b28; Manifest `versionTag` v4.41.1-b29, `hwrev` obelix_pvt; die Abbilder sind verschieden gelinkt, Kieselstrasse steckt in beiden, `WorkerState` ist in beiden 640 Byte |
| Bau | `skripte/bundle_b29.sh`, beide Slots ohne Warnung (`bau.txt`) |

**Nicht signiert und nicht veröffentlicht**, wie b27 und b28. Der Nutzer öffnet die `.pbz` in Boulder.

## Prüfung

| Datei | Inhalt |
|---|---|
| `pruefung.txt` | auf `d3d17b338` (Merge): acht Bauten ohne Warnung, Host 440/440, alle Werkzeuge grün (im Prüfbau nur die Fehlerprobe rot, wie vorgesehen) |
| `host.txt` | der volle Host-Lauf |
| `worker.txt` | `sizeof(WorkerState)`: b28 648, v4.41.1 640 (Platz 640); Kernel-Heap 72480 → 72512 B |
| `qemu-a/`, `qemu-b/` | zwei QEMU-Läufe auf `8075766d8`, alle Prüfungen grün: L1 höchstens 12,7 ms, erster Start (icount) 19,5 bzw. 20,5 ms, L2 24,4 bzw. 14,6 ms, L4 8,8 bzw. 5,9 ms, L6 0,02 ms; Kernel-Heap ohne Wachstum (frei 52496 → 52420 → 52420 B), kein Neustart |
| `endstand-bauten.txt` | die acht Bauten auf `8075766d8` (Ballast 72512 B), ohne Warnung |
| `skripte/` | Prüflauf, QEMU, Bau des Pakets |
