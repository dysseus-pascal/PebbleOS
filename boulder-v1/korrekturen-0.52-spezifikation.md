# Boulder 0.52.0 – Korrekturen nach 0.51 auf dem Telefon (Spezifikation)

Stand 06.10.2026.
- **Freigabe:** Der Nutzer hat die Punkte in `~/Documents/Claude/boulder-v1/0.52-inputs.md` freigegeben („ja und führe gleich die Korrekturen durch“), dazu die einmalige HRV-Reparatur.
- **Grundregeln:** wie in `~/Documents/Claude/boulder-v1/tagesband-0.51-spezifikation.md`, Abschnitt „Grundregeln“. Sie gelten unverändert: nichts an BLE und Benachrichtigungen, Health-Connect-Daten nie löschen, UI nur Englisch, keine Textwände, Baukasten/Farben/Formate, deutsche Bezeichner, Kommentare zum WARUM, kein catch ohne Log, Mutation je Kernaussage, PNG-Belege mit Robolectric.
- **Code-Landkarte:** `~/Documents/Claude/boulder-v1/karte-0.51.md`. Sie ist nach 0.51 teils veraltet, ergänzen.
- **Entwurf:** `~/Documents/Claude/boulder-v1/entwurf-erlebnis/Erlebnis-Herz.dc.html`, dort HRV und Blutdruck.
- **Arbeitskopie:** `~/Projects/boulder-052`, Zweig `korrekturen-0.52` von main f078f11b (0.51.0). Das Submodul whisper.cpp aus `~/Projects/Boulder` initialisieren, wie bei 0.51. Nach jedem Paket committen und `git push -u origin korrekturen-0.52`.

## P1 – Heart & Body und Health-Liste
1. **Body in Heart & Body.**
   - Auf Health fällt die eigene Zeile „Body“ weg. Die Zeile „Heart & Body“ bleibt unverändert, ohne zusätzliche Werte.
   - Auf der Seite Heart & Body steht vor der Trends-Zeile eine Karte „BODY“: Gewicht gross, Fettanteil, Änderung in 30 Tagen (dieselben Werte und Formate wie die bisherige Body-Zeile).
   - Ein Link „Body >“ führt zur bestehenden Körperseite, deren Funktionen damit erreichbar bleiben.
   - Ohne Körperdaten (keine Waage) fehlt die Karte, wie bisher die Zeile.
2. **Ein Weg zu den Trends.** Der Link „Trends >“ in der Karte „YOUR USUAL“ fällt weg. Die Zeile „Trends · What makes a difference“ unten bleibt.
3. **HRV als grünes Trendband wie bei Fitbit und Garmin, wie im Entwurf.**
   - Das Band ist eine durchgehende Fläche durch die Tagesmitten, keine Treppe je Tag und ohne Nähte.
   - Die Linie zwischen den Punkten ist neutral grau, wie im Entwurf.
   - Die Punkte sind grün im Band und bernstein daneben, gross mit Kartenrand; der jüngste Tag hat einen Ring.
   - **Band schon ab 3 Werten:** Das Band erscheint ab 3 Werten in den 30 Tagen vor dem Tag (bisher 7). Die Rechnung bleibt: Mittel ± Streuung, mindestens ±3.
   - **Urteil erst ab 7 Werten:** Punktfarbe und Kopf-Urteil („In your range“) kommen weiter erst ab 7 Werten (`HRV_LERNNAECHTE`). Davor sind die Punkte neutral, und das Band ist gleich gezeichnet.
4. **Blutdruck.**
   - Oberer und unterer Wert desselben Tages werden mit einem senkrechten Strich verbunden, wie bei Garmin: Spanne von unten bis oben, neutral, hinter den Punkten.
   - Dazu ein Trendband für oben und eines für unten, mit denselben Regeln wie bei HRV (Fläche, Band ab 3, Urteil ab 7).
   - Die Linien von Tag zu Tag verbinden weiter oben mit oben und unten mit unten.

## P2 – Tagesband: Bezugslinien hinter dem Puls
1. **Linien:**
   - Hinter der Pulslinie stehen der Ruhepuls als gestrichelte Linie in Herzrot (`Farben.puls`, gedämpft) und die Zonengrenzen als gepunktete Linien in ihrer Zonenfarbe (`Farben.pulszonen`).
   - Die Zonen werden genau so gerechnet wie auf der Herzseite: `Gesundheitsrechnung.pulsZonen` mit demselben Ruhe- und Maximalpuls.
2. **Skala der Pulszeile:**
   - Unten: Ruhepuls − 10, oder tiefer, falls der Tagestiefstwert darunter liegt.
   - Oben: die erste Zonengrenze über dem Höchstwert im Band, plus etwas Luft. So hat ein ruhiger Tag einen Massstab und wird nicht gestaucht.
   - Gezeichnet werden nur die Linien innerhalb der Skala.
3. **Zahlen:** Die Werte der Linien stehen fest am rechten Rand der Box, ausserhalb des wischbaren Inhalts, und bleiben beim Wischen sichtbar. Sie sind klein und grau wie die Achsen der App und überdecken das Ausblenden des Rands nicht.
4. **Zugänglichkeit:** Die Beschreibung des Bands nennt den Ruhepuls.

## P3 – HRV-Daten absichern und reparieren (riskant: Daten)
1. **Keine leere HRV mehr über eine vorhandene.** Kommt eine Nacht mit gleichem `beginnUtc` erneut und hat hrv=0 (oder kein Wert), während die gespeicherte Nacht eine HRV > 0 hat, bleibt die gespeicherte HRV. Alle anderen Felder werden wie bisher ersetzt; eine neue HRV > 0 ersetzt die alte. Die Stelle ist laut Untersuchung `speichereNachtTeil` / NachtDao REPLACE.
2. **Wochenbild:** `jeTag` nimmt für die HRV-Woche je Aufwachtag die längste Nacht *mit* HRV > 0. Für Schlaf und Anderes bleibt die Wahl, wie sie ist.
3. **Einmalige Reparatur.**
   - **Was:** Für gespeicherte Nächte der letzten 60 Tage mit hrv = 0 oder null liest Boulder seine EIGENEN HRV-Sätze aus Health Connect: Herkunft das eigene Paket, clientRecordId `boulder-nacht-hrv-<beginnUtc>`, wie sie `NachtPlatform.android.kt` schreibt. Ist ein Wert im gültigen Bereich (1..200), trägt Boulder ihn in die eigene Nacht ein.
   - **Nur lesen:** In Health Connect wird nichts geschrieben und nichts gelöscht.
   - **Einmal:** Die Reparatur läuft einmal pro Installation über eine Marke in den Einstellungen, ausserhalb der Verbindungs- und Benachrichtigungswege, und loggt, wie viele Nächte sie repariert hat.
   - **Ohne Berechtigung oder bei Fehler:** Log, keine Änderung, die Marke bleibt offen für den nächsten Start.
   - **Erwartung beim Nutzer:** Do 1.10. (Nacht ab 30.9., 53 ms) und Sa 3.10. (Nacht ab 2.10., 61 ms) kommen zurück, falls die Sätze noch in Health Connect stehen.
4. **Tests:**
   - Merge-Regel (alte HRV bleibt; neue HRV > 0 ersetzt).
   - Wahl in `jeTag`.
   - Reparatur: setzt nur bei 0/null, nur eigene Herkunft und passende Id, nur 1..200, nur einmal; ohne Berechtigung keine Änderung.
   - Jede Regel mit Mutation belegt.
   - Echte Room-Datenbank im Speicher, wie bei `SchritthalbstundeTest`.

## Abschluss
- **Version:** 0.52.0 (`BOULDER_VERSION`).
- **Release-Notiz:** `.github/release/0.52.0.md`, deutsch, erste Zeile Titel; jeder Satz belegt. Sie sagt ehrlich, dass So 4.10. und Mo 5.10. ohne HRV bleiben, weil die Uhr dort unter b24 keine lieferte.
- **Prüfen:**
  - Gesamttest wie bei 0.51, mit `--rerun`.
  - Keystore kopieren, Release-APK bauen und mit apksigner prüfen (Zertifikat 212e0ca4…).
- **PNG-Belege:** Health-Liste ohne Body, Heart & Body ganz (HRV mit 4 und mit 8 Werten, Blutdruck mit 1 und mit 5 Tagen), Tagesband mit Bezugslinien (ruhiger und bewegter Tag). Ablage unter `~/Documents/Claude/boulder-v1/belege-0.52/`.
- **Zusammenführen:** Merge, Tag und Release macht der Hauptagent nach Sicht der Bilder.
