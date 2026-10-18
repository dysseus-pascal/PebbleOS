# Nächte – Notizen des Nutzers

## 7.→8.10.2026 (b26, Messstufe 4)
- **Aufgestanden um 06:00** (Aussage des Nutzers). Boulder (aufwachen 06:00, Wecker) liegt damit richtig, Fitbits „out of bed 05:13“ ist falsch. Bei Vergleichen gilt Fitbit für die Aufwachzeit also nicht als Wahrheit.
- **Einschlafen:** Boulder 21:18 gegen Fitbit „went to bed“ 21:17.
- **Falsche Wachphasen:** Boulder zählt 111 min wach, Fitbit 27; REM 1:00 h gegen 2:17 h.
  - Drei Wachphasen mit Puls auf Schlafniveau (Mittel 50–52) und hoher HRV sind laut Fitbit Schlaf bzw. REM: 00:57–01:07, 02:43–03:27, 04:43–04:56.
  - Kandidat für eine Regel, die solche Wachphasen als Schlaf behält (Puls und HRV). Noch nicht entschieden.

## Fitbit-Bilder früherer Nächte (am 08.10. in den Uploads gefunden und hierher kopiert)
- **30.9.→1.10.** (`fitbit-2026-09-30.png`):
  - Fitbit: ins Bett 21:58, aus dem Bett 05:58; wach 1 h 06, unruhig 20 min, REM 1 h 30, leicht 3 h 56, tief 1 h 28, Effizienz 86 %.
  - Boulder damals (`boulder-2026-09-30.png`): 8:26 h, 20:08–06:08, Ruhepuls 54, HRV 53.
- **2.→3.10.** (`fitbit-2026-10-02.png`): Fitbit: ins Bett 22:29, aus dem Bett 06:27; wach 2 h 11, unruhig 32 min, REM 1 h 16, leicht 3 h 19, tief 1 h 12, Effizienz 73 %.
- **Boulder-Exporte dazu:** `~/Projects/pebbleos/tests/fixtures/activity/boulder/` (09-30, 10-03 = Nacht 2.→3.10., 10-05).

## 8.→9.10., 9.→10.10. und 12.→13.10.2026 (b26, Messstufe 4)
- Exporte und Fitbit-Bilder: `nacht-2026-10-08.csv`, `-09.csv`, `-12.csv` und `fitbit-2026-10-08.png`,
  `-09.png`, `-12.png`. Analyse: `analyse-2026-10-13.md`.
- **8.→9.10.:** Fitbit war 22:32 im Bett, erster Schlaf 22:39, Ende 05:42. Wach 17 min, REM 1 h 50,
  leicht 3 h 48, tief 1 h 15. Boulder schlief ein um 22:53 (Wälzregel), wachte auf um 06:00, 6 h 57.
  Die einzige Wachphase in der Nacht (01:40–01:50) sehen beide gleich.
- **9.→10.10.:** Fitbit war 23:12 im Bett, Ende 06:57. Wach 16 min, REM 1 h 28, leicht 4 h 51,
  tief 1 h 10. Boulder schlief ein um 21:34, das ist falsch: ein Wegnicken vor einem Aufwach-Schub bis
  23:12. Boulder wachte auf um 07:00, 7 h 02. Drei falsche Wachphasen (01:41, 02:34, 04:38; 46 min)
  mit Schlafpuls und hoher HRV, wie am 7.10.
- **12.→13.10.:** Fitbit war 21:38 im Bett, Ende 05:46. Wach 1 h 19, REM 1 h 20, leicht 4 h 28,
  tief 1 h 01. Boulder schlief ein um 21:42, wachte auf um 05:59, 6 h 36. Die großen Wachphasen
  (02:25–03:12, 04:03–04:45) sieht Fitbit auch, aber kürzer.
- Wecker am 9.10. und 13.10. um 06:00 (Aussage des Nutzers): Boulders Aufwachen stimmt, Fitbits Ende
  (05:42, 05:46) ist zu früh, wie am 7.10.
- 9.10. 22:50–23:04 (rund 260 Schritte): vermutlich ein Toilettengang; davor wach im Bett.
- Umgesetzt in V2 (13.10.): O2 und W30 (`12b28664`, `4ea6fb64`), siehe `analyse-2026-10-13.md`, Abschnitt 7.

## 13.→14.10.2026 (b26, Messstufe 4)
- Export und Fitbit-Bild: `nacht-2026-10-13.csv`, `fitbit-2026-10-13.png`. Analyse:
  `analyse-2026-10-14.md`.
- **Fitbit:** im Bett 21:32, erster Schlaf 21:39, Ende 05:23. Wach 9 min (beim Zubettgehen und um
  23:04), REM 1 h 58, leicht 4 h 16, tief 1 h 24. Die Summen ergeben 467 min, die Spanne 471.
- **Boulder:** eingeschlafen 21:37 (V2: 21:39), aufgewacht 05:46, 7 h 13. Ruhepuls 51, HRV 54.
- **Falsche Wachphase 03:14–04:03 (49 min):** Puls auf Schlafniveau, HRV 64/65/80 (Nacht 55), ein
  Pulswert 78 um 03:40. Fitbit: tief, leicht, ab 03:36 REM. H2 behebt sie nicht, weil die Uhr beim
  Umdrehen um 03:17 12 Schritte zählte. H2/20 (Gang erst ab 20 Schritten) behebt sie.
- **Morgen:** 05:23–05:29 Bewegung, dann 16 min still mit Puls 49–53, Aufwachen 05:46.
- **Nutzer (14.10.):** 03:14–04:03 geschlafen; aufgewacht um 05:45, aufgestanden um 06:00. Boulders
  Aufwachen stimmt, Fitbits Aufstehzeit (05:23) stimmt nicht.
- **Fitbits Aufstehzeit („Time out of bed“)** war in vier von fünf Nächten mit Angabe des Nutzers zu
  früh: 7.10. 05:13, 8.10. 05:42, 12.10. 05:46 (jeweils statt 06:00), 13.10. 05:23 (statt 05:45/06:00).
  Nur am 9.10. ohne Wecker stimmte sie (06:57, Boulder 07:00). Für das Aufwachen gilt deshalb die
  Angabe des Nutzers, nicht Fitbit.
- F20: A, F21: A: Ein Schub mit hoher HRV ohne Gang (aus dem VMC) bleibt Schlaf. Umgesetzt in V2
  `14e34d2e` (`analyse-2026-10-14.md`, Abschnitte 5 und 6).

## 16.→17.10.2026 (V2, Messstufe 4)
- Export und Fitbit-Bild: `nacht-2026-10-16.csv`, `fitbit-2026-10-16.png`. Analyse:
  `analyse-2026-10-17.md`.
- **Fitbit:** im Bett 01:38, erster Schlaf 01:46, Ende 06:57. Wach 48 min (01:38–01:46, 05:14–05:21,
  06:06–06:38), REM 42 min, leicht 3 h 08, tief 41 min, Unruhe 18 min.
- **Boulder (V2, Uhr):** eingeschlafen 22:14, aufgewacht 07:24, 7 h 48. Ruhepuls 51, HRV 65. Die Uhr
  rechnet wie V2 (0 von 720 Minuten anders).
- **22:14–01:25 nach den Minuten wach und unruhig:**
  - 20 Wälz-Regungen und 37 Lagewechsel je Stunde, VMC 0 nur in 20 % der Minuten.
  - 152 Schritte in 13 Minuten, also mehrmals kurz auf.
  - V2 sieht das als Schlaf: Die Bewegung bleibt unter Cole-Kripkes Schwelle, und die Wälzregel
    schiebt das Einschlafen höchstens 30 min weit (21:47 → 22:14).
- **Variante E1b** (eingeschlafen ist, wer in 30 min mindestens 20 ganz still liegt, geprüft nach den
  Schüben): Einschlafen 01:25, Schlaf 317 min. Keine ältere Nacht ändert sich.
- **Morgen:** Fitbit wach 06:06–06:38, Ende 06:57. Um 07:00–07:07 47 Schritte, danach bis 07:24 kaum
  Bewegung; Boulder endet um 07:24. Laut Nutzer stimmt Fitbits Ende.
- **H4 bestätigt:** Der Schub 04:00–04:10 (Schritte 12 + 12, Puls 82, HRV 78) bleibt Schlaf; Fitbit
  sieht dort Schlaf.
- **Nutzer (17.10.):** „fitbit is the correct one with yhe slerp time. I used the phone long into the
  night ling in bed“. Fitbits Schlafzeit (01:38–06:57) stimmt; bis dahin lag der Nutzer mit dem Handy im Bett.
- **Variante E2** (ein späterer Schlaflauf verlängert die Nacht nur, wenn drei Fünftel seiner Minuten
  still sind): Ende 06:57 statt 07:24, wie Fitbit. Keine ältere Nacht ändert sich. Offen als F37
  (A: E1b und E2, B: nur E1b, C: keins), `analyse-2026-10-17.md`, Abschnitt 9.
- **F37: A** (17.10.): E1b und E2 in V2. Bei der Umsetzung nachgestellt: nach der Stille gilt die
  Wälzregel noch einmal (Einschlafen 01:39 statt 01:25, Regel vom 6.10.: höchstens zehn Minuten neben
  Fitbit), und beim Aufwachen zählt nur der Anfang eines Laufs (die Hälfte seiner ersten
  Viertelstunde). 16.10. so 01:39 bis 06:57. Umgesetzt in V2 `688b3097`, Vorabversion b32
  (`analyse-2026-10-17.md`, Abschnitt 10).

## 17.→18.10.2026 (V2 b32, Messstufe 4)
- Export und Fitbit-Bild: `nacht-2026-10-17.csv`, `fitbit-2026-10-17.png`. Analyse:
  `analyse-2026-10-18.md`.
- **Fitbit:** im Bett 23:36, erster Schlaf 23:43, Ende 06:31. Wach 75 min (23:36–23:43, 03:24–03:41,
  04:18–04:28, 04:41–05:22), REM 1 h 16, leicht 3 h 41, tief 43 min, Unruhe 29 min.
- **Boulder (V2, Uhr):** eingeschlafen 23:38, aufgewacht 06:21, 6 h 43, keine Minute wach. Ruhepuls
  53, HRV 67. Die Uhr rechnet wie b32 (0 von 681 Minuten anders); b31 rechnet gleich.
- **Nutzer (18.10.):** „diese nacht sieht sehr ungleich aus. wir waren denke ich schon viel näher“.
- **Fitbits drei Wachphasen sieht die Uhr als Schlaf:** Puls beim Schlafpuls (49–62), nur um 05:10
  einmal 85, dazwischen Drehen alle paar Minuten.
  - 04:39–05:18 ist ein Aufwach-Schub (8 Regungen, Puls +32, HRV 89 gegen 63, kein Gang). H4 lässt ihn
    Schlaf. Er sieht aus wie die Schübe, die Fitbit bisher als Schlaf sah (13.10. auch laut Nutzer).
  - Ohne H4: diese Nacht 83 → 48 min Schlaf/Wach anders als Fitbit, alle Nächte 290 → 372.
  - Ausnahme für starke Pulsstöße: hilft nur bei 31–32 Schlägen und nur in dieser Nacht. Kein
    Vorschlag.
- **Nah an Fitbit** ist b32 in Nächten mit wenig Wachliegen (8., 9., 13.10.: 95–97 % gleich), weiter
  weg in Nächten mit viel (12., 16., 17.10.: 80–83 %).
- **Morgen:** 06:21 Regung mit Puls 86 (Boulders Aufwachen), 06:31 22 Schritte (Fitbits „out of
  bed“).
- **Phasen:** 65 % gleich, wo beide Schlaf sehen; REM 141 gegen 76 min.
- **Nutzer (18.10.):** „Ich Weiss es nicht mehr Aber ich denke ja.“ Die drei Wachphasen sind
  wahrscheinlich, nicht sicher; zum Morgen keine Angabe. V2 bleibt; H4 wird erst geändert, wenn
  weitere Nächte mit Fitbit einen Schub mit hoher HRV als wach zeigen (`analyse-2026-10-18.md`,
  Abschnitt 11).
