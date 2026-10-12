# Belege AP9 (Abnahme Schritt 3)

Bericht: `../../schritt3-abnahme.md`. Endstand: `v2` bei `ecb6b739`.

| Ordner/Datei | Inhalt |
|---|---|
| `bauten.txt`, `schluss_pruefen.sh` | acht Bauten mit Warnungen; Schlusslauf (Bauten, Host, Werkzeuge) |
| `host.txt` | voller Host-Lauf, 409/409 |
| `werkzeuge/` | Ausgaben aller Werkzeuge im Schlusslauf (`ergebnis.txt`: Rückgabewerte) |
| `qemu-a/`, `qemu-b/`, `qemu-c/` | drei QEMU-Schlussläufe: messung.json, Protokoll, Ausgabe, Bilder |
| `qemu-mutation/` | QEMU-Mutation für L1 und Kernel-Heap: Skript und Ergebnis |
| `mutationen.py`, `mutationen.txt` | 43 Mutationen auf dem Endstand; `mutationen_vorlauf.txt`: Vorlauf (Abschnitt 4) |
| `kaltstart/` | Untersuchung des ersten Starts nach dem Boot (Reihen, Kalibrierung, `-icount`) |
| `k2/` | Mehrwort-Speichern an der Syscall-Grenze (objdump), Befund K2 |
| `gegenpruefung-proben/` | Proben der drei Prüfer der Gegenprüfung (kern, app, inhalt) |
