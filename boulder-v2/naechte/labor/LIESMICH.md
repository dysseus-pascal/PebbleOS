# Nachtlabor: Boulder-Nächte auf dem Rechner nachrechnen und mit Fitbit vergleichen

`bewerten.py` und `wachphasen.py` finden ihre Daten relativ zu sich selbst (`labor/` und `naechte/`).
Die übrigen Skripte nehmen ihre Pfade als Argumente.

| Datei | Zweck |
|---|---|
| `nacht_rechnen.c` | Liest eine Boulder-CSV und rechnet sie mit `schlafanalyse.c`; vergleicht Kopf und Phasen mit der Uhr. Bauen: `gcc -O2 -I<v2>/include -o nacht_rechnen nacht_rechnen.c <v2>/fw/services/activity/boulder/schlafanalyse.c` |
| `schlafanalyse_var.c` | Kopie von `schlafanalyse.c` (b26) mit den Varianten, je per `-D` einschaltbar: `VAR_O1`, `VAR_O2`, `VAR_H1`, `VAR_H2`, `VAR_H3`, `VAR_H4`, `GANG_SCHRITTE=<n>`, `EINSCHLAF_RUHE=<min>`, `VAR_STILL=<min>` mit `STILL_FENSTER=<min>`, `VAR_STILL_SPAET` und `VAR_STILL_RUHE` (E1, E1b, E1c), `VAR_STILL_ENDE=<prozent>` und `VAR_STILL_KOPF=<min>` (E2; alle in `analyse-2026-10-17.md`), `VAR_H4_PULS=<bpm>` (P30, `analyse-2026-10-18.md`: ein Schub mit so viel Pulsstoss bleibt wach, auch bei hoher HRV). Ohne Schalter rechnet sie wie b26, mit `-DVAR_O2 -DEINSCHLAF_RUHE=30` wie V2 `4ea6fb64`, mit `-DVAR_O2 -DVAR_H4 -DEINSCHLAF_RUHE=30` wie V2 von `14e34d2e` bis b31, wie `v_o2h4w30e1ce2k` (unten) wie V2 seit `688b3097` (b32; beides an allen Nächten bis 16.10. geprüft). Bauen wie oben, mit `-I` auf die Header von V2 und `schlafanalyse_var.c` statt `schlafanalyse.c` |
| `fitbit_baender.py` | Findet die vier Zeilen (wach, REM, leicht, tief) in einem Fitbit-Bild |
| `fitbit_lesen.py` | Liest die Stufe je Minute aus dem Bild und gleicht sie auf Fitbits Summen an. Ergebnis: `fitbit-*.json` (960 Werte ab 20:00: −1 aus, 0 wach, 1 leicht, 2 tief, 3 REM) |
| `vergleich.py` | Boulder gegen Fitbit je Minute: Tabelle, Wachphasen, Streifen. Ausgabe `vergleich.txt` |
| `mit_rechnung.py` | Schreibt eine CSV mit Kopf und Phasen einer Nachrechnung, damit `vergleich.py` auch V2 oder eine Variante gegen Fitbit stellt |
| `wachphasen.py` | Merkmale aller Wachphasen in der Nacht. Ausgabe `wachphasen.txt` |
| `b32_alle-2026-10-18.txt` | Kopf von `vergleich.py` für alle Nächte mit Fitbit, mit V2 b32 nachgerechnet (`mit_rechnung.py` mit `v_o2h4w30e1ce2k`) |
| `schuebe.py` | Die Aufwach-Schübe einer Nacht: HRV gegen den Median der Nacht, Pulsstoss, stille Minuten, Lagewechsel und Regungen je Stunde, Fitbits Wachminuten. Liest die Ausgaben eines Prüfbaus: `schlafanalyse.c` mit `schub_ausgaben.patch`, gebaut wie `nacht_rechnen.c`; Aufruf im Kopf des Skripts. Ausgabe `schuebe-2026-10-18.txt` (Nächte 7. bis 17.10., Prüfbau auf `688b3097`) |
| `gegen_v2.py` | Welche Nächte eine Variante anders rechnet als V2 (`v_o2h4w30`) oder als `$GEGEN`, mit Einschlafen und Schlaf. Ausgaben `gegen_v2-2026-10-17.txt` (E1, E1b, E1c), `gegen_e1b-2026-10-17.txt` (E2 über den ganzen Lauf gegen V2 + E1b) und `gegen_e1c-2026-10-17.txt` (E2 mit Anfang gegen V2 + E1c) |
| `stille.py` | Anteil stiller Minuten (VMC 0, unter 50, 100, 250) in 30-Minuten-Fenstern um das Einschlafen und in Fitbits Schlaf; für den 16.10. gleitend. Ausgabe `stille-2026-10-17.txt` |
| `bewerten.py` | Alle Varianten über alle Nächte; die Programme liegen in `$NACHTLABOR_BIN` (sonst `labor/bin`). Mit `--wecker` zählt Fitbit bis zum Aufwachen, das der Nutzer angegeben hat: am 8. und 12.10. bis zum Wecker um 06:00, am 13.10. bis 05:45 (`fitbit-08k.json`, `fitbit-12k.json`, `fitbit-13k.json`). Ausgaben: `varianten*.txt` vom 13.10. (Nächte bis 12.10.), `varianten*-2026-10-14.txt` (mit dem 13.10. und H3), `varianten*-2026-10-17.txt` (mit dem 16.10. und E1b), `varianten*-2026-10-18.txt` (mit dem 17.10.) |
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
| `v_h4`, `v_o2h4w30` | `-DVAR_H4` (Gang aus dem VMC), dazu `-DVAR_O2 -DEINSCHLAF_RUHE=30` (= V2 seit `14e34d2e`) |
| `v_o2h4w30e1b` | wie `v_o2h4w30`, dazu `-DVAR_STILL=20 -DSTILL_FENSTER=30 -DVAR_STILL_SPAET` (E1b) |
| `v_o2h4w30e1be2` | wie `v_o2h4w30e1b`, dazu `-DVAR_STILL_ENDE=60` (E2 über den ganzen Lauf) |
| `v_o2h4w30e1c` | wie `v_o2h4w30e1b`, dazu `-DVAR_STILL_RUHE` (E1c) |
| `v_o2h4w30e1ce2k` | wie `v_o2h4w30e1c`, dazu `-DVAR_STILL_ENDE=50 -DVAR_STILL_KOPF=15` (= V2 b32) |
| `v_o2w30e1ce2k` | wie `v_o2h4w30e1ce2k`, aber ohne `-DVAR_H4` (b32 ohne die HRV-Regel der Schübe) |

Für `gegen_v2.py` am 17.10.: `v_e1b_<m>` = `v_o2h4w30` mit `-DVAR_STILL=<m> -DSTILL_FENSTER=30 -DVAR_STILL_SPAET`, `v_e1b_<m>v<w>` dasselbe mit `-DSTILL_FENSTER=<w>`, `v_e1_<m>` ohne `-DVAR_STILL_SPAET`, `v_e1c_<m>` mit `-DVAR_STILL_RUHE`, `v_e2_<p>` = `v_o2h4w30e1b` mit `-DVAR_STILL_ENDE=<p>`, `v_e2k_<p>` = `v_o2h4w30e1c` mit `-DVAR_STILL_ENDE=<p> -DVAR_STILL_KOPF=15`. Am 18.10.: `v_p<n>` = `v_o2h4w30e1ce2k` mit `-DVAR_H4_PULS=<n>` (`gegen_b32-2026-10-18.txt`; `v_p30` und `v_p31` über alle Nächte mit `bewerten.py` in `varianten_p-2026-10-18.txt`, dort auch mit `--wecker`).
