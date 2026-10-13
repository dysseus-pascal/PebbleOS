# Belege SDK 0.1.11 (GCC 15.2) auf dem Stand nach v4.41.0

Upstream verlangt seit v4.41.0 SDK 0.1.11 (`SDK_VERSION`). Bis `273728a3` war mit 0.1.10 (GCC 14.2)
gebaut worden. Hier dieselbe Prüfung mit 0.1.11, auf `10b0d9d8`/`b6a4b3df` (Firmware gleich wie
`273728a3`). Bericht: `../../schritt3-abnahme.md`, Abschnitt 9 (Nachtrag).

| Ordner/Datei | Inhalt |
|---|---|
| `bauten.txt` | acht Bauordner neu angelegt und sauber gebaut, ohne Warnung |
| `host.txt` | voller Host-Lauf, 437/437 |
| `werkzeuge/` | alle Werkzeuge, `pruefe_ohne_rechte` auf den neuen Abbildern, `mehrwort.txt` (K2) |
| `qemu-a/`, `qemu-b/` | zwei QEMU-Läufe (QEMU aus SDK 0.1.11), F17 B |
| `mutationen.txt` | 43/43 rot |
| `skripte/` | Neuanlage, Bau, Prüflauf, QEMU (SDK aus `SDK_VERSION`) |
