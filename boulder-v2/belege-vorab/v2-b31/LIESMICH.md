# V2 v4.41.1-b31: Velo nur mit Puls (16.10.2026)

b30 mit F32 (`../../entscheidungen.md`; Bericht `../../schritt3-abnahme.md`, Abschnitt 15): Die Uhr
fragt nur noch nach Velo, wenn im Fenster mindestens drei Pulswerte liegen und ihr Median mindestens
30 über dem Ruhepuls liegt (bisher +20, ohne Pulswerte ganz ohne Prüfung).

## Paket

| | |
|---|---|
| Datei | `normal_obelix_pvt_v4.41.1-b31.pbz`, 3 449 398 Byte |
| sha256 | `484ee31ee218405d33a8e75a08fdd83d208c6878df6c9f9ddb5f277a08010b0d` |
| Stand | V2 `b3d0828de` (Zweig `v2`), annotiertes Tag `v4.41.1-b31` (nur lokal) |
| Inhalt | `slot0/` und `slot1/` wie bei b30; Manifest `versionTag` v4.41.1-b31, `hwrev` obelix_pvt |
| Bau | `skripte/bundle_b31.sh`, beide Slots ohne Warnung (`bau.txt`) |

**Nicht signiert und nicht veröffentlicht**, wie b27 bis b30. Der Nutzer öffnet die `.pbz` in Boulder.

## Grundlage

Nachgestellt an den Wachminuten der echten Nächte aus `tests/fixtures/activity/boulder` (Abend vor
dem Einschlafen, Morgen nach dem Aufwachen; ohne `nacht-2026-10-05.csv`, die denselben Abend wie
`nacht-2026-10-05-morgen.csv` deckt, also 8 Tage):

| Datei | Inhalt |
|---|---|
| `velo-wachminuten.txt` | je Abend und Morgen: bewegte Minuten (VMC ≥ 250), davon velo-artig, Kandidatinnen-Fenster. Zusammen 656 von 723 velo-artig |
| `velo-fenster.txt` | die Fenster, die eine Kandidatin wären: 03.10. 07:15–07:26, 09.10. 07:48–07:59, 12.10. 20:00–20:13. Fast 0 Schritte, VMC-Median 1100–4000, gleiche Lage, höchstens ein Pulswert |
| `velo-puls.txt` | Puls über dem Ruhepuls in den velo-artigen Wachminuten mit Puls (233): Median +23, p75 +31, p90 +39 |
| `skripte/wach.py`, `fenster.py`, `puls.py` | die Nachstellung (Regeln wie `kontext.c` vor b31, Puls nur, wo er gemessen ist) |

Die positive Seite ist eine einzige echte Fahrt: 29.9.2026, 17:37–18:00, Puls 94–145 bei Ruhepuls 54
(`TrainingsanalyseTest.kt`, `FAHRT_PULS` in `test_boulder_fahrt.c`). Weitere Fahrten kommen mit dem
Minutenexport (F33).

## Prüfung

| Datei | Inhalt |
|---|---|
| `pruefung.txt` | auf `a3652f881` mit den drei geänderten Dateien, die danach `b3d0828de` wurden: acht Bauten ohne Warnung, Host 440/440, alle Werkzeuge grün (im Prüfbau nur die Fehlerprobe rot, wie vorgesehen) |
| `host.txt` | der volle Host-Lauf auf dem Commit `b3d0828de`: 440/440, DUMA wie üblich |
| `skripte/pruefen.sh` | wie b30 |

- **DUMA:** Ein erster Host-Lauf vor `pruefen.sh` brach in `test_kieselsport` mit „mprotect() failed“
  ab (SIGILL), auch auf `a3652f881` (b30) ohne diese Änderung. Der Container stand noch auf
  `vm.max_map_count` 65530; `pruefen.sh` hebt den Wert an (wie bei b30). Danach läuft die Suite durch.
  Der Lauf in `pruefung.txt` hatte zusätzlich `DUMA_PROTECT_FREE=0`, `host.txt` nicht.
- **Mutationen:** Schwelle wieder +20 macht `armarbeit_zu_hause_fragt_nicht` und
  `velo_ab_30_ueber_dem_ruhepuls` rot; Fenster ohne Puls wieder durchgelassen macht
  `ohne_puls_keine_velofrage` rot.
- **Nicht wiederholt:** die QEMU-Läufe. Die Änderung ist eine Bedingung je Minute ohne Zeitbedarf; Fahrt,
  Frage und Streichen deckt der Host (`test_boulder_fahrt`) mit dem echten Code.
