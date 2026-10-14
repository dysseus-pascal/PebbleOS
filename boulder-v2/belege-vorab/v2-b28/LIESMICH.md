# V2 v4.41.0-b28: Kieselstrasse (14.10.2026)

Kieselstrasse ist aus b26 zurück (F6, `../../schritt3-abnahme.md`, Abschnitt 12). Sonst wie b27
(`../v2-b27/`).

## Paket

| | |
|---|---|
| Datei | `normal_obelix_pvt_v4.41.0-b28.pbz`, 3 449 394 Byte |
| sha256 | `8bb7e9b0f20290dd0d14e7a717a1cd1475b77e0f7e5716ae59ea7640b11a14ec` |
| Stand | V2 `fef08c2e` (Zweig `v2`), annotiertes Tag `v4.41.0-b28` (nur lokal) |
| Inhalt | `slot0/` und `slot1/` wie bei b27; Manifest `versionTag` v4.41.0-b28, `hwrev` obelix_pvt; die beiden Abbilder sind verschieden gelinkt, Kieselstrasse steckt in beiden |
| Bau | `skripte/bundle_b28.sh`: wie b27, dazu geprüft auf `CONFIG_BOULDER_KIESELSTRASSE=y`. Beide Slots ohne Warnung (`bau.txt`). Das erste Tag war leicht; der Bau liest nur annotierte Tags und nannte sich `v4.41.0-b27-3-gfef08c2e`. Darum ist das Tag jetzt annotiert wie bei b27 |

**Nicht signiert und nicht veröffentlicht**, aus denselben Gründen wie b27 (kein Signaturschlüssel,
kein gültiges `gh`-Token, Release Boulder `v0.53.0` fehlt). Das Tag steht nur lokal. Der Nutzer bekommt
die `.pbz` direkt und öffnet sie in Boulder.

## Prüfung

| Datei | Inhalt |
|---|---|
| `pruefung.txt` | acht Bauten ohne Warnung, Host 440/440, alle Werkzeuge grün (im Prüfbau nur die Fehlerprobe rot, wie vorgesehen); Arbeitsstand mit dem Code von `d4e15214` |
| `host.txt` | der volle Host-Lauf |
| `mutationen.txt` | F6a–F6d, alle rot (`../../belege-schritt3/ap9/mutationen.py`) |
| `kieselstrasse/` | die Demo-Bilder von b26 und V2 (`blatt-*.png`), ihr Vergleich (`vergleich.txt`, `vergleich-00.png`) und die Protokolle |
| `qemu-a/`, `qemu-b/` | zwei QEMU-Läufe auf `fef08c2e`, alle Prüfungen grün: L1 höchstens 12,7 bzw. 17,6 ms, erster Start (icount) 19,5 ms, L2 18,6 ms, L4 8,8 ms, L6 höchstens 0,02 ms; Kernel-Heap ohne Wachstum (frei 52460 → 52384 → 52384 B, 256 B weniger als bei b27: der Ballast folgt jetzt 72480 B), kein Neustart |
| `endstand-bauten.txt` | die acht Bauten noch einmal auf `fef08c2e` (Ballast 72480 B), ohne Warnung |
| `skripte/` | Demo-Bau und -Bilder, Prüflauf, QEMU, Bau des Pakets |
