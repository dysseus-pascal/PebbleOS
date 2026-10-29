# V2 v4.43.0-b42: PebbleOS 4.43 und main (29.10.2026)

b41 mit Pebbles v4.43.0 und `main` bis `4f874566a` (`../../entscheidungen.md`, Abschnitt „PebbleOS
4.43 und Core 1.16.0.4“). Der Nutzer: „upsteam“, gewählt „Releases + FW main“. Dazu gehört Boulder
0.62.0; Mindest-App bleibt 0.61.0.

- **Merge** (V2 `b013cc1a5`): 176 Commits seit b41, ohne Konflikt; Git hat `activity_sessions.c` und
  `settings_blob_db.c` mit Boulders Haken zusammengeführt. Neu bei Pebble: Treiber der Pebble Time 2
  auf Pebbles Gerätemodell, `notifRespectPhoneSilence` (stille Zustellung, Bit 6 im Kopf des
  Timeline-Eintrags), App-Wecker beenden den Stationär-Modus, die Ruhezeit schaltet Alarme nicht
  mehr stumm, BLE 45–60 ms mit Latenz 4 (wirksam 300 statt 1400 ms).
- **Kraepelin-Kopie:** Upstream beendet eine laufende Geh- oder Laufsitzung, wenn ein Training die
  Auto-Erkennung abschaltet oder die Uhrzeit springt (`prv_end_step_activity`,
  `kalg_enable_activity_tracking` mit Rückruf, vier neue Tests). Im ersten Host-Lauf nach dem Merge
  waren deshalb `test_boulder_kraepelin_algorithm` (diese vier Tests) und
  `test_boulder_kraepelin_gleich` rot (`host-vor-kopie.txt`). Übernommen per `git merge-file`; D11
  sitzt jetzt in `prv_end_step_activity`. Pebbles Aufruf gibt `nullptr` als Kontext, Boulders
  Rückruf puffert dorthin (D1): `activity_algorithm_enable_activity_tracking` puffert auf dem Stack
  und meldet nach `prv_unlock()`. HERKUNFT nennt `4f874566a`.

## Paket

| | |
|---|---|
| Datei | `normal_obelix_pvt_v4.43.0-b42.pbz`, 3 514 555 Byte |
| sha256 | `139070b824d13ed30cffb62b46efc0f7e00390caec61f573667716c6efdd9cbb` |
| Stand | V2 `e5ab5baad` (Zweig `v2`): der Merge `b013cc1a5` und darauf nur `qemu_health.py` (pbl emulator screenshot); Tag `v4.43.0-b42` nur in einem Wegwerf-Klon |
| Inhalt | `slot0/` und `slot1/` wie bei b41; Manifest `versionTag` v4.43.0-b42, `commit` e5ab5baad, `hwrev` obelix_pvt (`manifest.txt`) |
| Bau | `skripte/bundle_b42.sh`, beide Slots ohne Warnung (`bau.txt`) |
| Release | v4.43.0-b42 in boulder-firmware (29.10.), Lauf 40952730039, Tag auf dem Release-Commit `5e537e3`; Digest des Release-Pakets wie gebaut, Signatur mit `signatur.pub` geprüft (`sig/`, derselbe Schlüssel wie in Boulders `FirmwareSignatur.kt`), Gegenprobe mit falscher Grösse rot |

## Prüfung

| Datei | Inhalt |
|---|---|
| `pruefung.txt` | acht Bauten ohne Warnung, alle Werkzeuge grün (im Prüfbau nur die Fehlerprobe rot, wie vorgesehen), QEMU-Abbild; auf `b013cc1a5` |
| `pruefung-vor-kopie.txt` | derselbe Lauf auf dem Merge vor dem Nachziehen der Kopie |
| `host.txt` | Host-Tests auf `b013cc1a5`: 481/481 (neu `test_boulder_glue__training_beendet_spaziergang` und Pebbles vier Gehtests in der Kopie) |
| `host-vor-kopie.txt` | der erste Host-Lauf: 479/481, rot die beiden Kraepelin-Tests der Kopie |
| `mutationen.txt` | drei eingebaute Fehler im neuen D1-Block, alle vom neuen Test `test_boulder_glue__training_beendet_spaziergang` erkannt |
| `bilder/` | QEMU (`skripte/qemu_pruefnacht.py`, Prüfbau `build-pruef` auf `b013cc1a5`): das Zifferblatt Tagesband nach einer Prüfnacht (`01_pruefnacht.png`, Ruhepuls 52, HRV 56) ist pixelgleich zu `../v2-b41/bilder/01_pruefnacht.png`; eine Minute später (`02_eine_minute_spaeter.png`). Wie bei b41 nahm der Prüfbau nach der Prüfnacht den Schlafmodus nicht an. Log `qemu_pruefnacht.txt`, Konsole `konsole.log` |
| `pruef/` | Logs der Werkzeuge |
| `skripte/` | `pruefen.sh` (Bauten, Werkzeuge), `host.sh`, `versionsrepo.sh`, `bundle_b42.sh`, `qemu_pruefnacht.py` |

- **Werkzeug:** Pebble hat `pbl screenshot` in `pbl emulator screenshot` aufgehen lassen (034c6e3c5).
  `tools/boulder/qemu_health.py` ruft den neuen Befehl (V2 `e5ab5baad`); er findet QEMU wie bisher
  über `qemu-mon.sock` im Bauordner.
- **Offen:** der Gerätetest, vor allem die neuen Treiber der Pebble Time 2 (Display, Touch, I2C,
  UART) und die stille Zustellung mit Boulder 0.62.
