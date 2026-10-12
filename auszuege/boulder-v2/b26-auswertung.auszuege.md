# Auszüge: `boulder-v2/b26-auswertung.md`

KEINE vollständige Datei. Die Datei selbst wurde von Workflow-Agenten geschrieben, deren Schreibvorgänge nicht im Transkript stehen. Hier stehen nur Befehle der alten Sitzung (Sitzung `session_011udJ51Jg5Y9cBcFvGfK26z`), die Teile davon gelesen haben, mit ihrer Ausgabe, wörtlich. Ausgaben können auch andere Dateien betreffen.

## 2026-10-06 05:28:23 (toolu_01N343QKW8f7Kvt8gGA2H8km)

Befehl:

~~~~
cd ~/Documents/Claude/boulder-v2 && head -c 3500 b26-auswertung.md; echo; echo =======; head -c 3500 b26-einbau.md
~~~~

Ausgabe:

~~~~
Exit code 1
# b26 – Auswertung F6/F9 (Schlafbeginn, Aufwachen)

Stand 06.10.2026. Ich habe nur gelesen und gerechnet, nichts committet. Das Repo `~/Projects/pebbleos` ist unverändert (`git status` leer).
Proben liegen unter `/tmp/claude-1000/-home-dysseus-Documents-Claude/a0d87cde-04f6-43b4-bc17-a2150aed9501/scratchpad/b26/probe/`.

## Kurzfassung

- **Eine Pulsbedingung (F6) trägt nicht.** Keine Einstellung der drei Puls- bzw. Ruhe-Regeln bringt alle vier Nächte auf ±10 min. Das gilt sogar dann, wenn man an allen vier Nächten zugleich einstellt. Alle drei Regeln brechen ausserdem den Test `einschlafen_nach_dem_herumwaelzen`. Der Grund: Beim Einschlafen laut Fitbit liegt der Puls noch bei 63–72, also 10–15 Schläge über dem Nachtmedian. Er fällt erst 1–2 h später. Am 05.10. lag er von 21:46 bis 22:15 bei 69 und danach ebenfalls bei 69.
- **F9 Aufwachen:** In b25 liegt keine Nacht mehr als 10 min daneben. Die Abweichungen sind 29.09. +5, 30.09. +10 (genau an der Grenze), 03.10. +3 und 05.10. 0. Hier muss nichts nachgestellt werden, und keiner der Kandidaten ändert das Aufwachen.
- **F9 Einschlafen:** Nur der 05.10. verfehlt (−29 min). Das behebt **Kandidat D, die nachgestellte Einschlaf-Regel (Herumwälzen)**: Das Fenster wächst von 20 auf 40 min, und als Wälz-Regung zählen 9/16 statt 5/16 der Wachschwelle. Damit liegen alle vier Nächte in ±10 und alle Untergrenzen halten (30.09. wird besser). Alle sechs Boulder-Test-Programme sind grün, der Schlafzustand liegt bei 300/300, und der Snapshot vom 26.09. bleibt unverändert.
- **Ehrlich zur Kreuzvalidierung:** Auch D trägt nicht robust an allen vier Prüfnächten. 29.09. und 03.10. halten in jeder Einstellung. Ob 30.09. und 05.10. halten, hängt am Gleichstandsbrecher: Jede dieser beiden Nächte liefert genau eine Bedingung, die in den anderen drei nicht steckt.
  - Für das Fenster ≥ 25 min gibt es nur einen Beleg, den 05.10.
  - Für die Schwelle ≥ 6/16 gibt es nur einen Beleg, den 30.09.
- **Empfehlung:** Kandidat D (Fenster 40 min, Wälz-Regung 9/16), aber **ohne Pulsbedingung**. Er hält als einziger alle vier Nächte und alle Tests und kostet keine andere Nacht eine Minute. Ein unabhängiger Beleg fehlt bisher:
  - Der Teil „9/16“ war vorab notiert (Kommentar in `test_boulder_schlaf.c`, „erst gegen die nächste Fitbit-Nacht prüfen“). Der 05.10. bestätigt ihn nur als neutral.
  - Der Teil „Fenster 40 min“ ist allein am 05.10. eingestellt. Erst die nächste Fitbit-Nacht prüft ihn unabhängig.

## 1. Die Rechnung und die Nacht 05.10.

- **Harness** (`probe/harness.c`): Er ruft die **echte** `schlafanalyse_werte()` auf, je nach Lauf aus b25 oder aus einer Kandidaten-Kopie, und bildet keine Regel nach. Er lädt die Nächte genau wie `prv_lade_nacht` im Test. Die Fitbit-Vergleiche, die Übereinstimmung und die Kreuzvalidierung rechnet Python (`auswertung.py`, `kv.py`, `kv2.py`).
- **Gegenprobe:** Der Harness liefert dieselben Zahlen wie die Tests (56,9 / 47,3 / 59,0 %). Für den 05.10. trifft er den Kopf des Uhr-Exports genau: Einschlafen 106, Aufwachen 600, Schlaf 479, Ruhepuls 55, HRV 50.
- **Die CSV vom 05.10. musste ich nicht umwandeln.** Sie hat dasselbe Format wie die Fixtures vom 30.09. und 03.10. (`minute,uhrzeit,schritte,vmc,puls,lage,phase`), dazu den HRV-Block `hrv_minute,uhrzeit,rmssd,puls` mit 33 Fenstern und 625 Minuten ohne Lücke. Bei b25 kommt mit HRV und ohne dasselbe heraus. Bei D ändert die HRV nur die Phasen (Tief 85 statt 91 min), nich
(eval):1: ====== not found
~~~~
