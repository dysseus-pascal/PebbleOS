# Erster Start nach dem Boot (L1): Untersuchung in AP9

**Anlass:** In den QEMU-Läufen nach der Gegenprüfung kam der erste Start nach dem Boot auf 49,8 und
50,8 ms (AP8: 32,2 ms, Grenze 50 ms). Alle weiteren Starts brauchten 5–15 ms.

## Reihen in Echtzeit-QEMU (Prüfbau, je Boot ein kalter Start)

Getauscht wurde nur der App-Code (`fw/apps/system/boulder_health/`), der Kern blieb gleich.

| Reihe | Code von AP8 | heutiger Code (`55d8e4c9`) | Code von `082171a9` |
|---|---|---|---|
| `ab_reihe.out` | 34,2 · 31,2 · 31,2 | 32,2 · 48,8 · 35,2 | – |
| `ab_steuerung.out` | – | – | 45,9 · 48,8 · 32,2 |
| `kal_reihe.out` | 32,2 · 31,2 · 38,1 · 30,3 | 43,9 · 31,2 · 53,7 · 31,2 | – |
| `fertig_reihe.out` | – | 43,9 · 34,2 · 35,2 | – |

- Die schnellen Läufe liegen bei beiden Fassungen um 31 ms, langsame gibt es bei beiden.
- Host-Kalibrierung (`kal_reihe.out`, `kal_vor`/`kal_nach`): Die Rechenzeit des Hosts erklärt die
  langsamen Läufe nicht.
- Zeitraffer der Prüfnacht (`fertig_reihe.out`): Er ist nach 1,7–2,2 s fertig, lange vor dem ersten
  Start. Daran liegt es also nicht.
- Schon die Strecke `main` → `start` schwankt zwischen 2 und 7 ms, obwohl dort in beiden Fassungen
  derselbe Code läuft: Startgrund lesen und eine Logzeile schreiben.

## Mit `-icount` (Gastzeit nach Befehlen, 8 ns je Befehl)

`icount_kaltstart.py`, `icount_reihe.out`, `icount_einzeln.out`. Mit `-icount` zählt QEMU die
Gastzeit nach ausgeführten Befehlen. Die Zeit, die QEMU zum Übersetzen braucht, fällt damit heraus.

| | erster Start | zweiter und dritter |
|---|---|---|
| Code von AP8 | 17,6 · 16,6 ms | 8,8 ms |
| heutiger Code | 17,6 · 17,6 ms | 8,8 ms |

`main` → `daten` dauert dort unter 1 ms. In Echtzeit-QEMU sind es 5–8 ms beim ersten Start und rund
1 ms bei den weiteren.

**Schluss:** Was die App beim ersten Start tut, ist seit AP8 gleich viel Arbeit. In Echtzeit-QEMU
kommt hinzu, dass QEMU jeden Codepfad beim ersten Lauf übersetzt. Dieser Anteil schwankt mit dem
Host, und auf der Uhr gibt es ihn nicht.
