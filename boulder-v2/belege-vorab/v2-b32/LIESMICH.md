# V2 v4.41.1-b32: Eingeschlafen ist, wer still liegt (17.10.2026)

b31 mit F37 (`../../entscheidungen.md`; Analyse `../../naechte/analyse-2026-10-17.md`, Abschnitt 10;
Bericht `../../schritt3-abnahme.md`, Abschnitt 16): zwei Regeln zur Stille (VMC 0) in
`schlafanalyse.c`.

- **Einschlafen:** In den 30 Minuten ab dem Einschlafen sind mindestens 20 ganz still. Geprüft nach
  den Aufwach-Schüben. Schiebt die Stille das Einschlafen, gilt ab dort die Wälzregel noch einmal.
  Ohne stille halbe Stunde bleibt das Einschlafen.
- **Aufwachen:** Ein späterer Schlaflauf verlängert die Nacht nur, wenn die Hälfte seiner ersten
  Viertelstunde still ist. Das gilt nur, wo ein Lauf der Nacht still beginnt.
- **Nacht 16.10.:** 01:39 bis 06:57 statt 22:14 bis 07:24. Fitbit: im Bett 01:38, Schlaf ab 01:46,
  Ende 06:57; vom Nutzer bestätigt (bis 01:38 mit dem Handy im Bett).

## Paket

| | |
|---|---|
| Datei | `normal_obelix_pvt_v4.41.1-b32.pbz`, 3 449 498 Byte |
| sha256 | `4681fd1ebe3dd3a4bfa993c49e1021e8717a0e0164b05bc5567332959958a26a` |
| Stand | V2 `688b30974` (Zweig `v2`), annotiertes Tag `v4.41.1-b32` (nur lokal) |
| Inhalt | `slot0/` und `slot1/` wie bei b31; Manifest `versionTag` v4.41.1-b32, `hwrev` obelix_pvt |
| Bau | `skripte/bundle_b32.sh`, beide Slots ohne Warnung (`bau.txt`) |

**Nicht signiert und nicht veröffentlicht**, wie b27 bis b31. Der Nutzer öffnet die `.pbz` in Boulder.

## Prüfung

| Datei | Inhalt |
|---|---|
| `pruefung.txt` | auf `688b30974`: acht Bauten ohne Warnung, Host 440/440, alle Werkzeuge grün (im Prüfbau nur die Fehlerprobe rot, wie vorgesehen und wie bei b31) |
| `host.txt` | der volle Host-Lauf daraus: 440/440 |
| `skripte/pruefen.sh` | wie b31 |

- **Tests:** `test_boulder_schlaf` hat die Nacht 16.10. mit Fitbits Phasen, das Handy im Bett und
  den unruhigen Morgen an gebauten Nächten sowie dieselbe Nacht ohne VMC 0 (dann wie bis b31). Ein
  Fall rechnet mit Absicht anders: `gang_in_der_letzten_stunde`, 30.9. mit Gang 35 min vor dem
  Aufwachen (die Nacht endet am Gang).
- **Mutationen:** Zwölf werden rot, eine ist nicht erkannt (zehn statt fünfzehn Minuten Anfang;
  beide liegen im belegten Bereich). Die Liste steht in der Analyse, Abschnitt 10.
- **Labor:** `v_o2h4w30e1ce2k` (`../../naechte/labor/`) rechnet alle Nächte Minute für Minute wie
  `688b30974`.
- **Rechenzeit:** Auf dem Rechner rechnet b32 die Nacht 16.10. schneller als b31 (480 statt 800 µs,
  die Nacht ist kürzer) und den 13.10. gleich schnell (570–620 µs). Die neuen Schleifen fallen neben
  den Phasen nicht ins Gewicht.
- **Nicht wiederholt:** die QEMU-Läufe, wie bei b31.
