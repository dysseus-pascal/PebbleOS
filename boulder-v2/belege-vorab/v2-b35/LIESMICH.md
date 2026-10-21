# V2 v4.41.2-b35: Karten gleiten wie bei Pebble (21.10.2026)

b34 mit F40 (`../../entscheidungen.md`; Bericht `../../schritt3-abnahme.md`, Abschnitt 19):
„Können wir in Health vertikal scrollen?“ – „A. Wir bleiben mit allem was wir machen so nahe an
Pebble wie es geht“.

- **Gleiten** (V2 `685b50fe9`): Die Health-Karten gleiten beim Blättern wie in Pebbles eigener
  Health-App, mit Pebbles Kurve und Dauer (264 ms). Oben bringt die Karte von oben, Unten von unten,
  auch beim Wischen. Die Details öffnen wieder mit Pebbles Übergang.
- **Messung:**
  - V2 `967da424b`: QEMU zählt 1000 Ticks je Sekunde, `qemu_health.py` rechnete mit 1024 und zeigte
    jede Zeit 2,4 % zu klein.
  - V2 `a9133d56e`: L2 begrenzt den Anteil der App. Wann das Neue zu sehen ist, bestimmen Pebbles
    Animationen.
- **Probe** (V2 `e044bda27`): Seit b34 (Upstream `main`) braucht der Abbau einer MenuLayer für eine
  App mit System-ID Systemrechte. Die Health-App benutzt keine; die Probe kommt jetzt ohne aus.

## Paket

| | |
|---|---|
| Datei | `normal_obelix_pvt_v4.41.2-b35.pbz`, 3 467 804 Byte |
| sha256 | `8911a19df5a67c6fdf2267940bd852211a9d24fa8076d8ae283beb3b09f68317` |
| Stand | V2 `a9133d56e` (Zweig `v2`), annotiertes Tag `v4.41.2-b35` (nur lokal) |
| Inhalt | `slot0/` und `slot1/` wie bei b34; Manifest `versionTag` v4.41.2-b35, `hwrev` obelix_pvt; Abbilder verschieden gelinkt |
| Bau | `skripte/bundle_b35.sh`, beide Slots ohne Warnung (`bau.txt`) |

**Nicht signiert und nicht veröffentlicht**, wie b27 bis b34. Der Nutzer öffnet die `.pbz` in Boulder.

## Prüfung

| Datei | Inhalt |
|---|---|
| `pruefung.txt` | auf `e044bda27` (`a9133d56e` ändert nur `qemu_health.py`): acht Bauten ohne Warnung, Host 466/466, alle Werkzeuge grün (im Prüfbau nur die Fehlerprobe rot, wie vorgesehen) |
| `host.txt` | der volle Host-Lauf daraus |
| `mutationen.txt` | alle 62 Mutationen (`../../belege-schritt3/ap9/mutationen.py`), darunter F40a–g und K2c neu: rot |
| `qemu-a/`, `qemu-b/` | zwei QEMU-Läufe auf `a9133d56e`, alle Prüfungen grün: L1 höchstens 16 bzw. 14 ms, erster Start (icount) 26,0 ms; L2 (Anteil der App) 18 bzw. 13 ms; die Karte ruht nach 265–278 ms, die neue ist nach 35–70 ms zu sehen, das Detail nach 52,5 bzw. 55,5 ms im Median; L4 7 bzw. 6 ms, L6 0,02 ms; Kernel-Heap ohne Wachstum (51 408 → 51 332 → 51 332 B), App-Heap höchstens 5 748 B, Syscall-Stapel mit 1 368 B Reserve, kein Neustart |
| `vorlauf-l2/` | der erste QEMU-Lauf auf `d63ebcc6e` (L2 als Zeit, bis das Neue zu sehen ist, Grenze 63 ms): rot an L2 mit 70 ms; daraus `a9133d56e` |
| `probe/` | die Probe der Bausteine ohne Systemrechte in QEMU: 44 Bausteine, darunter das Gleiten und Pebbles Kurve, „vollständig“, kein Fehler |
| `gleiten/` | Bilder mitten im Gleiten (`skripte/gleiten_bilder.py`); `m_3_up.png` ist zerrissen, weil die Aufnahme mitten ins Zeichnen fiel |
| `skripte/` | Prüflauf, QEMU, Probe, Bilder, Bau des Pakets |

- **Kernel-Heap:** Im Release-Bau von obelix hat der Kernel wie bei b34 70 432 Byte Heap; der Prüfbau
  legt so viel Ballast an, dass ihm gleich viel bleibt.
- **Bilder:** Karten und Details gleichen denen von b34 Pixel für Pixel, ausser der SpO2-Zeile im
  Heart-Detail (7 Pixel, sie zeigt die letzten 24 Stunden und rückt mit der Uhrzeit des Laufs).
