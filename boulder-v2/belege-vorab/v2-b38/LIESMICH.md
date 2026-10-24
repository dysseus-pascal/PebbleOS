# V2 v4.41.2-b38: Unterarten in Kieselsport (24.10.2026)

b37 mit F50 und F51 (`../../entscheidungen.md`, Abschnitt „Unterarten in Kieselsport“). Der Nutzer:
„In kieselsport möchte ich unterarten im biking menu. gravel/road, MTB, indoor“, „schwimmen bekommt
indoor und open water“, „open water und open water gps“, „Mach noch custom und custom gps für treking
ohne bestimmten sport typ“. Dazu gehört Boulder 0.59.0.

- **Menü** (V2 `637c6fd5e`): Running, Bike, Hiking, Strength, Yoga, Swimming, Breathing, Custom,
  Custom GPS. „Bike“ öffnet Road/Gravel, MTB, Indoor; „Swimming“ Pool, Open water, Open water GPS.
  Die Menüs ordnen sich selbst (`art_text.c`), beide Gruppen teilen ein Untermenü.
- **Neue Arten, hinten angefügt:** 8 Indoor, 9 Open water, 10 Open water GPS, 11 Custom, 12 Custom GPS.
  Art 6 heisst jetzt Pool. Ob das Telefon GPS aufzeichnet, sagt allein die Zahl (10 und 12).
- **Messen:** Indoor wie Road/Gravel (offenes Training, Modell für hohe Belastung, ohne Schritte).
  Open water mit und ohne GPS wie das Becken, aber ohne Bahnen und Kompass. Custom mit und ohne GPS
  wie Pebbles „Workout“; die Schritte zählen wie ohne Training.
- **Bilder und Farben:** Indoor Rad im Haus (Rajah), Open water Schwimmer vor zwei Gipfeln
  (VividCerulean, mit GPS BlueMoon), Custom Pebbles Herz (Folly, mit GPS FashionMagenta).

## Paket

| | |
|---|---|
| Datei | `normal_obelix_pvt_v4.41.2-b38.pbz`, 3 472 666 Byte |
| sha256 | `a68e1cf7848d7bd9ed1268395079058ac12b444aa6806a3cbce4a095b725ab51` |
| Stand | V2 `637c6fd5e` (Zweig `v2`); Tag `v4.41.2-b38` nur in einem Wegwerf-Klon |
| Inhalt | `slot0/` und `slot1/` wie bei b37; Manifest `versionTag` v4.41.2-b38, `commit` 637c6fd5e, `hwrev` obelix_pvt (`manifest.txt`) |
| Bau | `skripte/bundle_b38.sh`, beide Slots ohne Warnung (`bau.txt`) |

## Prüfung

| Datei | Inhalt |
|---|---|
| `pruefung.txt` | auf `637c6fd5e`, sauberer Baum: Host 471/471, acht Bauten ohne Warnung, alle Werkzeuge grün (im Prüfbau nur die Fehlerprobe rot, wie vorgesehen), QEMU-Bilder |
| `mutationen.txt` | 54 Mutationen (`skripte/mutationen.py`), alle rot; danach alles gebaut und grün |
| `bilder/` | QEMU (`skripte/menue_bilder.py`): Hauptmenü, Bike und Swimming mit allen Arten, die Startschirme von Indoor, Open water GPS, Custom und Custom GPS, Zurück, zuletzt wieder Bike (das geteilte Untermenü zeigt die Räder) |
| `pruef/` | Logs der Schlussprüfung |

- **Neue Tests:** `test_kieselsport_arten` hält die Zahlen aller Arten fest (bis b37 prüfte das kein
  Test) und prüft die Menüs, Gruppen, Obertitel, Namen, Bilder und Farben; `test_kieselsport` +4
  (Indoor, Open water ohne und mit GPS, Custom). Die V1-Tabellen in `test_kieselsport` und
  `test_kieselsport_folgen` führen die neuen Arten.
- **Indoor-Bild:** Die erste Fassung (Wände bei ±12) lief in QEMU auf 26 und 64 Punkten mit den
  Rädern zusammen; jetzt stehen die Wände am Rand des Rasters.
- **Spanisch:** Open water GPS heisst „Abiertas GPS“, damit „GPS“ im Menü der Pebble Time 2 ganz zu
  sehen bleibt.
