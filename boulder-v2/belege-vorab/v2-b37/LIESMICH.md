# V2 v4.41.2-b37: SpO2 nach Schaltern (22.10.2026)

b36 mit F44 und F45 (`../../entscheidungen.md`, Abschnitt „SpO2 nach Schaltern“; Bericht
`../../schritt3-abnahme.md`, Abschnitt 21): „für spo2 möchte ich in den einstellungen checkboxen für
schlaf, sport oder intervall. das spart akku.“ Dazu gehört Boulder 0.58.0 mit dem Schalter „Blood
Oxygen During Sleep“.

- **Schlüssel `boulderSpo2Schlaf`** (V2 `762802af9`): auf der Whitelist, gelesen im Rückruf der
  Messstufe, bei seinem eigenen BlobDB-Ereignis, bei jeder Ablage und nach dem Menü. Fehlt er oder
  taugt er nicht, ist er aus.
- **„Blood Oxygen“ bleibt beim Nutzer** (V2 `49e0da937`): Die Messstufe setzt nur noch die Takte für
  Puls und SpO2, nicht mehr den Schalter. Stufe 1 misst SpO2 stündlich, wenn ein Schalter an ist.
- **Pebbles HRM-Manager** (V2 `260cfb33a`): „Schlaf“ erlaubt SpO2 und darf den Sensor einschalten,
  wie Pebbles Schalter für Trainings; zwei Blöcke unter dem Variantenschalter, Pebbles Zeilen
  unverändert. Ohne das sähe Pebbles Plan nachts nie einen Wert. K1 erlaubt in `hrm_manager.c` jetzt
  24 statt 10 Zeilen, nur dort und in `hrm_manager_new_data_cb`.
- **Pebbles Plan misst mit „Schlaf“ in der Nachtruhe** (V2 `9cb336b3d`): `spo2_plan.c`
  (`intervall || (schlaf && nachtruhe)`), Nachtruhe wie der Nachtpuls aus `puls.c`, in `activity.c`
  sieben Zeilen unter dem Variantenschalter. Takt, Vortritt für Puls und Training, flach und nicht
  am Arm bleiben Pebbles Plan.
- **Health-App** (V2 `b4e86c5f0`): Die Herzkarte zeigt SpO2 „off“ nur ohne beide Schalter.
- **Einstellungen der Uhr** (V2 `f4f4baed6`): Zeile „Blood O2 During Sleep“ direkt nach „Blood
  Oxygen“, nur im Boulder-Bau.
- **Sport:** kein Code; Kieselsport startet seine Trainings über Pebbles Workout-Dienst, damit misst
  Pebbles „Blood O2 During Activity“ etwa alle 5 Minuten bis 35 s (Puls pausiert dabei).

## Paket

| | |
|---|---|
| Datei | `normal_obelix_pvt_v4.41.2-b37.pbz`, 3 471 291 Byte |
| sha256 | `1267f683600f6309b6185448d298b8dfb01c359c1b8d12384ff868b8abe66816` |
| Stand | V2 `f4f4baed6` (Zweig `v2`); Tag `v4.41.2-b37` nur in einem Wegwerf-Klon |
| Inhalt | `slot0/` und `slot1/` wie bei b36; Manifest `versionTag` v4.41.2-b37, `commit` f4f4baed6, `hwrev` obelix_pvt |
| Bau | `skripte/bundle_b37.sh`, beide Slots ohne Warnung (`bau.txt`) |

## Prüfung

| Datei | Inhalt |
|---|---|
| `pruefung.txt` | auf `f4f4baed6`, sauberer Baum: acht Bauten ohne Warnung, Host 471/471 (b36: 468), alle Werkzeuge grün (im Prüfbau nur die Fehlerprobe rot, wie vorgesehen) |
| `mutationen.txt` | 29 Mutationen (`skripte/mutationen.py`), alle rot |
| `bilder/`, `qemu-lauf.txt` | QEMU: Menüzeile an und aus; tags 14:00 nur „Schlaf“: in 13 min kein SpO2-Abo; nachts 23:00 ruhig: SpO2 nach 64 s, endet nach gut einer Minute; „Schlaf“ aus: in 12 min keins. Der Pulssensor in QEMU ist ein Stub, gezeigt wird das Umschalten des Abos, keine echten Werte |
| `pruef/` | Logs der Schlussprüfung |

- **Neue Tests:** `test_boulder_spo2_plan` (die Regel in allen acht Fällen, dazu Pebbles echter
  Plan mit der Variante: tags nie, nachts im Takt), `test_boulder_einstellungen` (Menüzeile,
  Schreiben, Anzeige vor dem ersten Lesen; ohne Variante Pebbles Menü unverändert),
  `test_boulder_messung` +7, `test_boulder_schnittstelle` +1.
- **Beim Update:** Wer auf Stufe 2–5 war, hat „Blood Oxygen“ noch an (die alte Messstufe setzte
  es) und misst weiter den ganzen Tag, bis er es in Boulder ausschaltet.
