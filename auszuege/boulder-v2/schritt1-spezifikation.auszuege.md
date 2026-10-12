# Auszüge: `boulder-v2/schritt1-spezifikation.md`

KEINE vollständige Datei. Die Datei selbst wurde von Workflow-Agenten geschrieben, deren Schreibvorgänge nicht im Transkript stehen. Hier stehen nur Befehle der alten Sitzung (Sitzung `session_011udJ51Jg5Y9cBcFvGfK26z`), die Teile davon gelesen haben, mit ihrer Ausgabe, wörtlich. Ausgaben können auch andere Dateien betreffen.

## 2026-10-08 04:42:02 (toolu_01Nc4aLmqUpM4Th2KrhLPWJZ)

Befehl:

~~~~
cd ~/Documents/Claude/boulder-v2 && grep -n -i 'fitbit' b26-auswertung.md schritt1-spezifikation.md schlaf-schritte-gegenueberstellung.md schritt1-abnahme.md | cut -c1-260 | head -25
~~~~

Ausgabe:

~~~~
schlaf-schritte-gegenueberstellung.md:9:- **Phasen:** Garmins eigene Studie meldet κ 0,54, unabhängige Tests messen κ 0,21–0,25. Boulder liegt heute gegen Fitbit bei 54 % Übereinstimmung und κ 0,31. Direkt vergleichen lässt sich das nicht, weil Fitbit kein Sch
schlaf-schritte-gegenueberstellung.md:66:- Das Hypnogramm ist Minute für Minute dasselbe wie in b25. Probe am PC: 0 abweichende Minuten auf allen drei Fitbit-Nächten, auch mit eingebautem Wachliegen von 60–150 min.
schlaf-schritte-gegenueberstellung.md:111:- Dieser Vorschlag hat die meiste neue Logik und braucht die meisten Daten: 10–20 weitere Fitbit-Nächte, für Stufe B mit RR-Werten. Mit den heutigen Fixtures lässt sich Stufe B nicht trainieren.
schlaf-schritte-gegenueberstellung.md:115:- «Garmin-Niveau» lässt sich nur gegen Fitbit prüfen, und Fitbit ist selbst ungenau.
schlaf-schritte-gegenueberstellung.md:167:| Zusätzliche Daten nötig | kaum | 10–20 Fitbit-Nächte, RR | mindestens 10 Nächte |
schlaf-schritte-gegenueberstellung.md:189:3. **V2.2:** die Rechnung über die ganze Nacht aus Vorschlag 2 (Normierung, Uhrzeit-Merkmale, HMM), sobald genug Fitbit-Nächte da sind. Eine neue Regel kommt nur dazu, wenn sie auf getrennten Nächten gewinnt.
schlaf-schritte-gegenueberstellung.md:202:2. **Fitbit-Nächte:** Trägst du noch 10–20 Nächte den Fitbit mit, wenn möglich mit Datenexport statt Bildern? Ohne diese Nächte lässt sich eine Verbesserung nur behaupten, nicht belegen.
schritt1-spezifikation.md:161:tests/fixtures/activity/boulder/      b25-Nächte + Golden-Bytes + neue (Fitbit, Velo, Auto)
schritt1-spezifikation.md:411:- Ergebnis aus der PC-Probe (Fitbit-Nächte):
schritt1-spezifikation.md:508:- **Platzbedarf je Nacht:** ≤ 3 P + ≤ 10 C + ≤ 8 R. Auf den Fitbit-Nächten sind es 2/3/4 Zyklen und 5/7/3 Tiefabschnitte (PC, scratchpad/v2s1/zyklen.c). Die Sitzungen einer Nacht liegen bis zur folgenden Mitternacht in der Liste; 
schritt1-spezifikation.md:761:| K3 | **T3.1** Nachlauf = Ganznacht (0 Minuten). **T3.2** Golden-Bytes = b25 (AP-G), durch Boulders Parser-Tests. **T3.3** Fitbit, gerechnet auf der V2-Kette (Zyklen **und** 0x4001): ein/auf ±10 min an allen Nächten mit Fitbit-Ze
schritt1-spezifikation.md:762:| K4 | **T4.1** Live: Anteil der Schlafminuten mit Zustand ≠ Awake ≥ 95 % je Nacht; Bezug Fitbit, wo vorhanden, sonst b25-Endfassung; berichtet wird auch „wach live schlaf“. **T4.2** Wecker mit echter alarm.c: Weckzeiten 04:00–07:
schritt1-spezifikation.md:769:- ≥ 3 neue Fitbit-Nächte auf der V2-Firmware (K3), echte Weckmorgen (K4).
schritt1-spezifikation.md:778:1. Fitbit-Einschlafen und -Aufwachen für die Nacht 25./26.9., falls vorhanden.
schritt1-spezifikation.md:779:2. ≥ 3 neue Nächte, **jetzt schon mit b25 aufgenommen**: Fitbit-Export mit Schlafstufen (nicht als Bild) und Boulder-Nachtexport. Sie gehen sofort in T3.3 auf dem Host, nicht erst nach Schritt 3.
schritt1-spezifikation.md:869:**F9: Wenn die b25-Regeln eine neue Nacht verfehlen.** K3 verlangt ±10 min gegen Fitbit. Die Regeln sind bisher an vier Nächten geprüft, an der vom 3.10. auch eingestellt.
schritt1-spezifikation.md:898:| Fitbit-Referenzen 29.9., 30.9., 3.10. | aus Bildern abgelesen bzw. geschätzt; 26.9. ohne Fitbit-Zeiten |
schritt1-spezifikation.md:968:| K M8 Fitbit-Referenz schwach | 9 Daten, T3.3, F9 |
schritt1-abnahme.md:24:| 3 | Schlaf: Testnächte, ≥3 neue gegen Fitbit, ±10 min, Summe über Zyklen | **teilweise** | T3.4 (`2ad9b7d9d`): Auf 6 Nächten ist die Zyklensumme gleich dem Schlaf (472/472/460/337/451/451). T3.2 (`9b74dbeb6`): 0x4001/0x4002 sind auf al
schritt1-abnahme.md:25:| 4 | Live ≥95 %, Smart Alarm nie im Tiefschlaf, Nickerchen | **teilweise** | T4.1: Live 96,2 / 97,7 / 98,3 / 97,0 / 100 / 100 %. Maßstab ist die fertige Nacht, nicht Fitbit. T4.2 (`04ef7ad34`, echter `alarm.c`): 258 Weckzeiten, keine we
schritt1-abnahme.md:36:2. Mindestens 3 neue Nächte gegen Fitbit: Einschlafen und Aufwachen ±10 min, mindestens so gut wie b25 (K3).
b26-auswertung.md:8:- **Eine Pulsbedingung (F6) trägt nicht.** Keine Einstellung der drei Puls- bzw. Ruhe-Regeln bringt alle vier Nächte auf ±10 min. Das gilt sogar dann, wenn man an allen vier Nächten zugleich einstellt. Alle drei Regeln brechen ausserdem den
b26-auswertung.md:15:  - Der Teil „9/16“ war vorab notiert (Kommentar in `test_boulder_schlaf.c`, „erst gegen die nächste Fitbit-Nacht prüfen“). Der 05.10. bestätigt ihn nur als neutral.
b26-auswertung.md:16:  - Der Teil „Fenster 40 min“ ist allein am 05.10. eingestellt. Erst die nächste Fitbit-Nacht prüft ihn unabhängig.
b26-auswertung.md:20:- **Harness** (`probe/harness.c`): Er ruft die **echte** `schlafanalyse_werte()` auf, je nach Lauf aus b25 oder aus einer Kandidaten-Kopie, und bildet keine Regel nach. Er lädt die Nächte genau wie `prv_lade_nacht` im Test. Die Fitbit-Verg
~~~~
