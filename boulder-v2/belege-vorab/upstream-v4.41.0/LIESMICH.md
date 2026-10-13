# Belege Upstream v4.41.0 (F18 A, vor der ersten Vorabversion)

Bericht: `../../schritt3-abnahme.md`, Abschnitt 9. Stand: `v2` bei `273728a3`.

| Ordner/Datei | Inhalt |
|---|---|
| `bauten.txt` | acht Bauten sauber neu gebaut, ohne Warnung |
| `host.txt` | voller Host-Lauf, 437/437 |
| `werkzeuge/` | Ausgaben aller Werkzeuge (`ergebnis.txt`: Rückgabewerte) |
| `qemu-a/`, `qemu-b/` | zwei QEMU-Läufe mit F17 B: messung.json, Protokoll, Ausgabe, Bilder |
| `mutationen.txt` | 43 Mutationen auf `273728a3`: 42 rot, G-A2 baute unter `-Werror` nicht |
| `mutation_g-a2.txt` | G-A2 umgeformt (`if (false)` statt Komma-Ausdruck), einzeln rot |
| `skripte/` | Prüflauf, QEMU-Läufe, sauberer Bau |
