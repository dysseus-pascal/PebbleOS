# V2 v4.41.2-b36: Atemübung in Kieselsport (22.10.2026)

b35 mit der Atemübung (F41–F43, `../../entscheidungen.md`; Bericht `../../schritt3-abnahme.md`,
Abschnitt 20): „Dazu möchte ich eine einstellbare pranayama atemübung in kieselsport“. Dazu gehört
Boulder 0.57.0, das den Rhythmus einstellt und die Übung als Training speichert.

- **Art Atmen** (V2 `efe80fc50`): Art 7, hinten angehängt; Lungen als Symbol, Kobaltblau. Misst Puls
  und HRV wie Yoga, ohne Schritte, Strecke und GPS.
- **Rhythmus vom Telefon** (V2 `26d640de9`): Schlüssel 10041–10045, im Dienst unter Persist 13–17.
  Einatmen und Ausatmen 1–60 s, Halten 0–60 s, 1–99 Runden, Vorgabe 4-4-4-4 mit 10 Runden. Die
  Meldung der Einstellungen hängt die fünf Werte hinten an; alles davor bleibt bytegleich zu b25.
- **Takt** (V2 `2a394a0da`, `atem.c`): Phase, Runde und Rest aus dem Rhythmus und der Zeit seit dem
  ersten Einatmen. Der Kreis wächst und schrumpft mit Pebbles Kurve (`animation_timing_curve`).
- **Übung** (V2 `8b69f42fd`): Startschirm mit Rhythmus und Dauer, Atemseite mit Kreis, Sekunden und
  Runde, ein Brummen je Phase. Nach der letzten Runde Pause und Speichern wie von Hand.
- **Pin** (V2 `4103a43fe`): Die Grenze folgt ArtAnzahl, 8 ist Atmen.
- **Shell** (V2 `214a624cd`): `kieselsport atem <ein> <halten> <aus> <leer> <runden>` für QEMU.

## Paket

| | |
|---|---|
| Datei | `normal_obelix_pvt_v4.41.2-b36.pbz`, 3 471 138 Byte |
| sha256 | `4e010300ec8946f0d8c063a37de47ffd09b0ff5733114cfaae5a954fb2f7f615` |
| Stand | V2 `214a624cd` (Zweig `v2`); Tag `v4.41.2-b36` nur in einem Wegwerf-Klon, er gehört auf `214a624cd` |
| Inhalt | `slot0/` und `slot1/` wie bei b35; Manifest `versionTag` v4.41.2-b36, `commit` 214a624cd, `hwrev` obelix_pvt |
| Bau | `skripte/bundle_b36.sh`, beide Slots ohne Warnung (`bau.txt`) |

**Nicht signiert und nicht veröffentlicht**, wie b27 bis b35. Der Nutzer öffnet die `.pbz` in Boulder.

## Prüfung

| Datei | Inhalt |
|---|---|
| `pruefung.txt` | auf `214a624cd`, sauberer Baum: acht Bauten ohne Warnung, alle Werkzeuge grün (im Prüfbau nur die Fehlerprobe rot, wie vorgesehen). Der Host-Lauf darin ist rot: `test_boulder_waechter_gegenproben` scheiterte an `git init` auf voller Platte |
| `host.txt` | der Host-Lauf nach dem Aufräumen: 468/468 (b35: 466; dazu `test_kieselsport_atem` und `test_kieselsport_arten`) |
| `mutationen.txt` | 47 Mutationen (`skripte/mutationen.py`), alle rot; nach dem Rückbau die sieben betroffenen Tests grün. S1 (Symbol fehlt) fängt der Bau (`-Werror`), S1b (Symbol leer) der Test |
| `bilder/` | QEMU (Prüfbau), 10 Runden 4-4-4-4, Puls 64: Menü, Startschirm, Countdown, jede Phase, Runde 2, Pulsseite, Ende, gespeichert; `uebersicht.png`; `ablauf.txt` mit der Zeit jeder Aufnahme. Gespeichert mit 160 s = 10 × 16 s |
| `varianten/` | A: 12 Runden 3-0-3-0, kein Halten, gespeichert mit 72 s. B (Öffnen nach dem Ende) ist dort nicht zu sehen: Pebbles Zusammenfassung des Trainings A lag über der App (`b_geoeffnet.png`) |
| `nach_dem_ende/` | B im frischen Emulator: 31 Runden 1-0-1-0 (62 s) aus der Shell, die App erst nach 66 s geöffnet; sie beendet die Übung gleich, gespeichert mit 1:06 |
| `skripte/` | Prüflauf, Bilder, Varianten, Öffnen, Mutationen, Bau des Pakets |

- **Ohne Messung mit `qemu_health.py`:** b36 ändert die Health-App nicht. Kieselsport läuft mit
  Systemrechten; `pruefe_ohne_rechte.py` ist für die drei Bauten grün.
- **Das Ende wartet auf den Dienst:** Dessen Sekunden laufen der Uhr der App bis gut eine Sekunde
  nach. Pause und Speichern gehen erst hinaus, wenn er die ganze Zeit gezählt hat (höchstens 5 s).
  So speichert die Uhr Runden × Rundendauer, und Boulder rechnet daraus die Runden. Der Pausenschirm
  zeigt dabei unter einer Sekunde lang eine Sekunde weniger (2:39 in `bilder/ende.png`).
- **Öffnen nach dem Ende:** gespeichert wird die aktive Zeit des Dienstes, auch wenn sie länger ist
  (66 s für 62 s). Boulder begrenzt die Runden auf die eingestellten.
- **HRV in QEMU:** Der Pulsgeber liefert RR-Abstände nur für Trainings aus der Shell; die Bilder
  aus der App zeigen darum „--“. HRV für Atmen prüft der Host-Test `atem_misst_wie_yoga`.
