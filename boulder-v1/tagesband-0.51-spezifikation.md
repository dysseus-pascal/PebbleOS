# Boulder 0.51.0 – neues Health-Erlebnis (Spezifikation)

Stand 06.10.2026. Vom Nutzer freigegebener Entwurf: Design-Leinwand https://claude.ai/artifact/XA6yEafnSX9vBCcr4crwUE, Seite „Erlebnis“. Lokale Kopie der Flächen (HTML, Masse und Farben direkt ablesbar): `~/Documents/Claude/boulder-v1/entwurf-erlebnis/Erlebnis-*.dc.html`. Die Werte in den Entwürfen sind Beispielwerte. In der App kommt alles aus den echten Daten.

Arbeitskopie: `~/Projects/boulder-051`, Zweig `tagesband-0.51` (von main 224bcf3d = 0.50.0). Nach jedem Paket committen und `git push -u origin tagesband-0.51`.

## Grundregeln (gelten für jedes Paket)
- **Unverändert bleibt:**
  - BLE-Verbindung, Reconnect, Benachrichtigungen und Notification-Listener.
  - Die Health-Connect-Daten des Nutzers; nie etwas löschen.
  - Alles, was hier nicht genannt ist („nicht gewählt bleibt“).
- **Texte und Darstellung:**
  - UI-Texte nur Englisch, über die bestehenden String-Ressourcen.
  - Keine Erklärtexte und keine Legenden: nur Zahlen, Urteile und kurze Labels.
  - Nur der bestehende Baukasten: Farben aus `ansicht/Farben.kt`, Formate aus `kiesel/Formate.kt` und `ansicht/Formathilfen.kt`, Bausteine aus `ansicht/Baukasten.kt`.
  - Diagrammregeln der App: Raster durchgezogen, Achse rechts, Ziel durchgezogen, Mittel gestrichelt, Zonen gepunktet.
- **Code:**
  - Bezeichner deutsch; Kommentare erklären das WARUM.
  - Kein `catch` ohne Log.
- **Tests:**
  - Tests bauen den Produktionscode nicht nach.
  - Jede Kernaussage wird mit einer Mutation geprüft: die Regel absichtlich brechen, der Test muss fallen, danach zurücksetzen.
- **Belege:** Jeder geänderte Bildschirm wird mit Robolectric und echter Grafik als PNG gezeichnet, nach dem Muster von `PulsKarteBildTest`, mit Beispieldaten. Die Pfade der PNGs kommen in den Bericht.
- **Umgebung:** `JAVA_HOME=/usr/lib/jvm/java-25-openjdk ANDROID_HOME=/opt/android-sdk ANDROID_USER_HOME=/tmp/claude-1000/-home-dysseus-Documents-Claude/a0d87cde-04f6-43b4-bc17-a2150aed9501/scratchpad/android-home`.
- **Gesamttest:** `./gradlew :pebble:testAndroidHostTest :composeApp:testAndroidHostTest :androidApp:testDebugUnitTest :libpebble3:testAndroidHostTest :libpebble3:jvmTest :util:testAndroidHostTest :androidApp:assembleRelease -q`

## A – Health-Start (`Erlebnis-Health`, `Erlebnis-Eintrag`)
1. **Kopf:** Die Datumszeile über dem Gruss fällt weg. Gruss und Satz bleiben.
2. **Tagesband statt Zeitstrahl:** Es ersetzt die bisherige Zeitstrahl-Karte („Today so far“), Titel bleibt „TODAY SO FAR“.
   - **Box:** quer wischbar.
     - Massstab: 40 dp je Stunde.
     - Ränder links und rechts blenden aus.
     - Beim Öffnen steht „jetzt“ bei etwa 75 % der sichtbaren Breite.
   - **Zeitfenster:**
     - `von` = Einschlafen der letzten Hauptnacht − 15 min. Ohne Nacht: heute 00:00 − 2 h.
     - `bis` = das Spätere von `von` + 24 h, jetzt + 1 h und „Schlafenszeit heute“ + 15 min. Die Schlafenszeit ist die, die der bisherige Zeitstrahl schon blass zeigte.
   - **Zeilen von oben nach unten:**
     1. Stunden alle 3 h, mit dünnen Rasterlinien.
     2. Pulslinie aus den vorhandenen Pulsdaten, in Herzrot `Farben.puls`. Bei Lücken über 15 min wird die Linie unterbrochen.
     3. Eine gemeinsame Zeile für Schlaf und Schritte, denn sie überschneiden sich zeitlich nie.
        - Tagsüber: Schrittsäulen je 30 min in `Farben.bewegung`, unten bündig.
        - Nachts: Hypnogramm der Hauptnacht mit 4 Spuren (Wach oben, REM, Leicht, Tief) in den Phasenfarben der Schlafansicht. Es ist so hoch wie Schrittsäulen und Symbolzeile zusammen.
     4. Symbole für Einträge: Kaffee/Koffein, Wasser/Glas, Präparat, Training/Spaziergang. Jedes ist ein runder Kreis mit Strich-Symbol, darunter der Wert in kleiner grauer Schrift („63 mg“, „0.25 l“, „3 of 4“, „2.7 km“).
     5. Unter der Nacht steht der Schlaftext in derselben Schrift wie die Werte: „7:59 h · resting 55 · HRV 50“. Werte, die fehlen, entfallen.
   - **Jetzt:** eine grüne senkrechte Linie mit der Uhrzeit oben. Daneben steht der letzte Puls nur, wenn die letzte Messung höchstens 15 min alt ist. Grund: Seit 0.35 zeigt „Jetzt“ keinen veralteten Puls.
   - **Zukunft:** blass hinterlegt. Geplantes erscheint als blasses Symbol mit Rand: noch offene Präparate zur geplanten Zeit, die Schlafenszeit als Mond.
   - **Dichte Einträge:**
     - Gleicher Typ innerhalb 45 min: zu einem Symbol zusammenfassen, Werte summieren (z. B. „0.5 l“).
     - Verschiedene Typen, die sich überdecken würden: das spätere Symbol bis zu 24 dp nach rechts schieben.
     - Reicht das nicht: zu einem Symbol „+N“ bündeln, das beim Tippen die Einträge zeigt.
   - **Gesten:**
     - Tipp auf die Nacht oder den Schlaftext öffnet die Schlafansicht.
     - Langer Druck auf ein eigenes Getränk, Glas oder Präparat öffnet das bestehende Ändern/Löschen. Dieses Verhalten der App bleibt.
     - Tipp auf Wasser öffnet Water.
     - Tipp auf Kaffee oder Präparat öffnet Stack.
     - Tipp auf Training öffnet dessen Detail.
   - **Zugänglichkeit:** Das Band hat eine Inhaltsbeschreibung. Die Liste darunter bleibt die zugängliche Form.
3. **Sport-Feld** direkt unter dem Band, Stil wie die Listenzeilen. Läufer-Symbol, „Sport“, darunter das letzte Training (Sportart, Strecke oder Dauer, relativer Tag). Rechts 7 Punkte für die Tage dieser Woche (dieselbe Definition wie die Woche der Sportseite, Trainingstage gefüllt) und „N this week“. Tipp öffnet die Sportseite.
4. **Liste darunter:**
   - **„Heart & Body“:** die bisherige Herzzeile, nur umbenannt. Inhalt und Werte bleiben; Tipp öffnet die Herzseite.
   - **„Water“:** unverändert, Tipp öffnet Water.
   - **„Stack“ ersetzt die bisherigen Zeilen Präparate und Koffein.**
     - Kopf: Pillen-Symbol, „Stack“, darunter die offenen Präparate in Bernstein („Magnesium still open“) oder ein ruhiges „All taken“.
     - Darunter drei Werte: „x of y supplements“, „N mg caffeine now“, „~M mg at bedtime“. Die Koffeinwerte kommen aus dem bestehenden Koffeinmodell.
     - Teile ohne Daten entfallen. Ohne beides entfällt die Zeile, wie bisher ohne Drinktervall/SupCycle.
5. **Navigation:**
   - Der Reiter Sport verschwindet aus der unteren Leiste; es bleiben 5 Einträge.
   - Die Sportseite wird eine Unterseite von Health: oben ein Zurück-Pfeil, die Android-Zurück-Taste führt zu Health.
   - Sonst bleibt die Sportseite exakt wie sie ist.
   - Jeder Weg, der bisher den Sport-Reiter öffnete (Benachrichtigung, Deep Link, Trainingsende, Widgets), öffnet jetzt die Sport-Unterseite.

## B – Schlafansicht (`Erlebnis-Schlaf`)
1. Die Stages-Karte fällt weg.
2. Unten in der Karte mit dem Hypnogramm („THROUGH THE NIGHT“) steht eine Zeile mit 4 Spalten in der Reihenfolge der Spuren: Wach, REM, Leicht, Tief. Jede Spalte hat Farbpunkt, Name und Dauer.
3. Der Rest der Schlafansicht bleibt.

## C – Heart & Body (`Erlebnis-Herz`)
1. **Titel:** „Heart & Body“.
2. **Pulsband:** bleibt, wie es ist (Zonenfarben, Nachtschatten).
3. **HRV:** als 7-Nächte-Bild im Stil von Garmin/Fitbit, mit dem bestehenden `HrvWoche`.
   - Das persönliche Band ist eine Fläche, die Nächte sind verbundene Punkte: grün im Band (`Farben.imBand`), bernstein daneben (`Farben.ausserBand`).
   - Kopf: Wert der letzten Nacht, 7-Nächte-Schnitt, Urteil.
   - Ist das schon so: nur prüfen.
4. **Blutdruck:** dieselbe Darstellung.
   - 7 Tage, je ein Band für den oberen und den unteren Wert.
   - Die Bänder werden nach derselben Methode wie das HRV-Band berechnet, getrennt für oben und unten.
   - Punkte grün im Band, bernstein daneben.
   - Ohne genug frühere Messungen: neutral wie bei HRV.
   - Kopf: heutiger oder letzter Wert mit Zeit und Schnitt.
5. **Rest:** SpO2, Ruhepuls, Body-Zeile und Körperseite bleiben.
6. **Trends:** Am Ende steht eine Zeile „Trends · What makes a difference“, die zur bestehenden Trendansicht führt.

## D – Water und Stack (`Erlebnis-Wasser`, `Erlebnis-Stack`)
1. **Water:**
   - Gibt es schon eine Wasserseite, wird sie angepasst; sonst neu.
   - Inhalt: Menge „0.5 of 2.5 l“ mit Fortschrittsbalken, Anzahl der Gläser, die Gläser von heute (langer Druck ändert oder löscht), 7 Tage als Säulen mit durchgezogener Ziellinie.
   - Die „nächste Erinnerung“ nur, wenn sie aus den Daten bekannt ist.
2. **Stack:** neue Seite.
   - **Supplements:**
     - Jedes Präparat mit Haken und Zeit (genommen) oder Bernstein-Ring und geplanter Zeit (offen).
     - Kurinfo aus SupCycle, falls vorhanden („week 3 of 8“).
   - **Caffeine:**
     - Die Werte „N mg active now“ und „63 mg today“.
     - Kurve aus dem bestehenden Koffeinmodell von morgens bis zur Schlafenszeit: bis jetzt durchgezogen mit Fläche, danach gestrichelt.
     - Jetzt-Linie grün; Schlafenszeit gepunktet mit „~M mg at HH:MM“.
     - Darunter die Koffein-Getränke von heute; langer Druck ändert oder löscht.
   - Bisherige eigene Seiten für Präparate oder Koffein gehen in Stack auf; ihre Funktionen bleiben erreichbar.

## Abschluss
- **Version:** 0.51.0, nach dem bestehenden Muster.
- **Release-Notiz:** `.github/release/0.51.0.md`, deutsch, erste Zeile ist der Titel. Jeder Satz der Notiz braucht einen Beleg.
- **Signatur:** Vor dem Release-Build `ci/debug.keystore` nach `$ANDROID_USER_HOME/debug.keystore` kopieren und mit apksigner prüfen (Zertifikat 212e0ca4…).
- **Zusammenführen:** Merge nach main, Tag und GitHub-Release macht der Hauptagent nach Sicht der PNGs.
