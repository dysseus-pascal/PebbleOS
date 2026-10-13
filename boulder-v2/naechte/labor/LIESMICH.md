# Nachtlabor: Boulder-Nächte auf dem Rechner nachrechnen und mit Fitbit vergleichen

Die Skripte tragen oben die Pfade der Sitzung vom 13.10.2026 (`/tmp/claude-0/...`); für einen neuen
Lauf dort anpassen.

| Datei | Zweck |
|---|---|
| `nacht_rechnen.c` | Liest eine Boulder-CSV und rechnet sie mit `schlafanalyse.c`; vergleicht Kopf und Phasen mit der Uhr. Bauen: `gcc -O2 -I<v2>/include -o nacht_rechnen nacht_rechnen.c <v2>/fw/services/activity/boulder/schlafanalyse.c` |
| `schlafanalyse_var.c` | Kopie von `schlafanalyse.c` mit den Varianten, je per `-D` einschaltbar: `VAR_O1`, `VAR_O2`, `VAR_H1`, `VAR_H2`, `EINSCHLAF_RUHE=<min>`. Ohne Schalter rechnet sie wie b26 |
| `fitbit_baender.py` | Findet die vier Zeilen (wach, REM, leicht, tief) in einem Fitbit-Bild |
| `fitbit_lesen.py` | Liest die Stufe je Minute aus dem Bild und gleicht sie auf Fitbits Summen an. Ergebnis: `fitbit-*.json` (960 Werte ab 20:00: −1 aus, 0 wach, 1 leicht, 2 tief, 3 REM) |
| `vergleich.py` | Boulder gegen Fitbit je Minute: Tabelle, Wachphasen, Streifen. Ausgabe `vergleich.txt` |
| `wachphasen.py` | Merkmale aller Wachphasen in der Nacht. Ausgabe `wachphasen.txt` |
| `bewerten.py` | Alle Varianten über alle Nächte. Ausgabe `varianten.txt` |
| `ref-*.csv` | Die b26-Referenzsätze aus `v2` (`tests/fixtures/activity/boulder/referenz-b26/`) als CSV; fehlende Minuten fehlen auch hier |
