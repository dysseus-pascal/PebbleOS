# V2 v4.41.2-b34: Upstream v4.41.2 und main (20.10.2026)

b33 mit F39 (`../../entscheidungen.md`; Bericht `../../schritt3-abnahme.md`, Abschnitt 18):
„Alles aus dem upstream übernehmen“.

- **v4.41.2** (V2 `97ae1df5`): Menüzeilen in Pebbles eigenen Menüs wieder 50 statt 61 px. Drei
  Benachrichtigungs-Änderungen aus v4.41.0 sind zurückgenommen.
- **Upstream `main`** (V2 `d5cc0038`, bis `723a1708c`, 395 Commits nach v4.41.0), mit Boulders
  Anpassungen an Upstreams Umbauten (Syscalls, Zeitstruktur, Asserts, Lichtsensor, Test-Stubs).

## Paket

| | |
|---|---|
| Datei | `normal_obelix_pvt_v4.41.2-b34.pbz`, 3 467 066 Byte |
| sha256 | `956b987b760b59b64c3ad1337549972dbd17241cd3a8c3ffc778496db021fb83` |
| Stand | V2 `d5cc0038c` (Zweig `v2`), annotiertes Tag `v4.41.2-b34` (nur lokal) |
| Inhalt | `slot0/` und `slot1/` wie bei b33; Manifest `versionTag` v4.41.2-b34, `hwrev` obelix_pvt; Abbilder verschieden gelinkt |
| Bau | `skripte/bundle_b34.sh`, beide Slots ohne Warnung (`bau.txt`) |

**Nicht signiert und nicht veröffentlicht**, wie b27 bis b33. Der Nutzer öffnet die `.pbz` in Boulder.

## Prüfung

| Datei | Inhalt |
|---|---|
| `pruefung.txt` | auf `d5cc0038c`: acht Bauten ohne Warnung, Host 466/466, alle Werkzeuge grün (im Prüfbau nur die Fehlerprobe rot, wie vorgesehen) |
| `host.txt` | der volle Host-Lauf daraus |
| `mutationen.txt` | alle 54 Mutationen (`../../belege-schritt3/ap9/mutationen.py`): rot |
| `qemu-a/`, `qemu-b/` | zwei QEMU-Läufe auf `d5cc0038c`, alle Prüfungen grün: L1 höchstens 13,7 ms, erster Start (icount) 23,4 ms (b33: 20,5), L2 11,7 bzw. 14,6 ms, L4 6,8 bzw. 5,9 ms, L6 0,01 ms; Kernel-Heap ohne Wachstum (frei 51 408 → 51 332 → 51 332 Byte), Reserve des Syscall-Stapels 1 368 Byte, kein Neustart |
| `skripte/` | Prüflauf, QEMU, Bau des Pakets |

- **Kernel-Heap:** Im Release-Bau von obelix hat der Kernel 70 432 Byte Heap, 2 304 weniger als b33.
  Der Prüfbau legt so viel Ballast an, dass ihm gleich viel bleibt (`CONFIG_BOULDER_PRUEFLAUF_HEAP`).
- **K2:** Seit `PBL_SYSCALL` kann der Rumpf eines Syscalls als eigene Funktion stehen
  (`sys_boulder_lage__impl.isra.0`). `mehrwort.sh` prüft diese Rümpfe mit; G-K2 wird genau dort rot.
