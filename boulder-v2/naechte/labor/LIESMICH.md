# Nachtlabor: Boulder-Nächte auf dem Rechner nachrechnen und mit Fitbit vergleichen

`bewerten.py` und `wachphasen.py` finden ihre Daten relativ zu sich selbst (`labor/` und `naechte/`).
Die übrigen Skripte nehmen ihre Pfade als Argumente.

| Datei | Zweck |
|---|---|
| `nacht_rechnen.c` | Liest eine Boulder-CSV und rechnet sie mit `schlafanalyse.c`; vergleicht Kopf und Phasen mit der Uhr. Bauen: `gcc -O2 -I<v2>/include -o nacht_rechnen nacht_rechnen.c <v2>/fw/services/activity/boulder/schlafanalyse.c` |
| `schlafanalyse_var.c` | Kopie von `schlafanalyse.c` (b26) mit den Varianten, je per `-D` einschaltbar: `VAR_O1`, `VAR_O2`, `VAR_H1`, `VAR_H2`, `VAR_H3`, `GANG_SCHRITTE=<n>`, `EINSCHLAF_RUHE=<min>`. Ohne Schalter rechnet sie wie b26, mit `-DVAR_O2 -DEINSCHLAF_RUHE=30` wie V2 (`4ea6fb64`). Bauen wie oben, mit `-I` auf die Header von b26 (`12b28664`) und `schlafanalyse_var.c` statt `schlafanalyse.c` |
| `fitbit_baender.py` | Findet die vier Zeilen (wach, REM, leicht, tief) in einem Fitbit-Bild |
| `fitbit_lesen.py` | Liest die Stufe je Minute aus dem Bild und gleicht sie auf Fitbits Summen an. Ergebnis: `fitbit-*.json` (960 Werte ab 20:00: −1 aus, 0 wach, 1 leicht, 2 tief, 3 REM) |
| `vergleich.py` | Boulder gegen Fitbit je Minute: Tabelle, Wachphasen, Streifen. Ausgabe `vergleich.txt` |
| `mit_rechnung.py` | Schreibt eine CSV mit Kopf und Phasen einer Nachrechnung, damit `vergleich.py` auch V2 oder eine Variante gegen Fitbit stellt |
| `wachphasen.py` | Merkmale aller Wachphasen in der Nacht. Ausgabe `wachphasen.txt` |
| `bewerten.py` | Alle Varianten über alle Nächte; die Programme liegen in `$NACHTLABOR_BIN` (sonst `labor/bin`). Mit `--wecker` zählt Fitbit am 8. und 12.10. bis zum Wecker um 06:00 (`fitbit-08k.json`, `fitbit-12k.json`). Ausgaben: `varianten*.txt` vom 13.10. (Nächte bis 12.10.), `varianten*-2026-10-14.txt` (mit dem 13.10. und H3) |
| `ref-*.csv` | Die b26-Referenzsätze aus `v2` (`tests/fixtures/activity/boulder/referenz-b26/`) als CSV; fehlende Minuten fehlen auch hier |

Die Programme der Varianten in `bewerten.py`:

| Name | Schalter |
|---|---|
| `v_b26` | – |
| `v_w30` | `-DEINSCHLAF_RUHE=30` |
| `v_o2` | `-DVAR_O2` |
| `v_h2`, `v_o2h2` | `-DVAR_H2`, dazu `-DVAR_O2` |
| `v_o2w30` | `-DVAR_O2 -DEINSCHLAF_RUHE=30` (= V2) |
| `v_o2h2w30` | `-DVAR_O2 -DVAR_H2 -DEINSCHLAF_RUHE=30` |
| `v_h3`, `v_o2h3w30` | `-DVAR_H3`, dazu `-DVAR_O2 -DEINSCHLAF_RUHE=30` |
| `v_g20`, `v_o2g20w30` | `-DVAR_H2 -DGANG_SCHRITTE=20` (H2/20), dazu `-DVAR_O2 -DEINSCHLAF_RUHE=30` |
| `v_o2g25w30` | `-DVAR_O2 -DVAR_H2 -DGANG_SCHRITTE=25 -DEINSCHLAF_RUHE=30` |
| `v_o2hnw30` | `-DVAR_O2 -DVAR_H2 -DGANG_SCHRITTE=100000 -DEINSCHLAF_RUHE=30` (HRV allein, ohne Gang) |
