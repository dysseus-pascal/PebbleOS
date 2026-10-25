# Boulder V2 – Entscheidungen

Stand: 05.10.2026. Quelle: Diskussion mit dem Nutzer. Unterlagen zu den Schlaf-/Schrittvorschlägen in diesem Ordner.

## Grundlage
- **Grundsatz (Nutzer, 21.10.2026): „Wir bleiben mit allem was wir machen so nahe an Pebble wie es
  geht. Merk dir das.“** Er gilt für Firmware, Uhr-Apps und App. Wo Pebble (Core Devices) etwas
  schon kann oder vormacht, übernimmt Boulder dessen Verhalten, Aussehen und Code-Wege. Eigene Wege
  braucht es nur, wo Pebble nichts bietet. Bei Vorschlägen und Fragen an den Nutzer kommt die Lösung
  nächst an Pebble zuerst.
- **Ergänzung (Nutzer, 25.10.2026): „ändere aber unseren code so das wir immer so nahe an der
  vorlage wie möglich bleiben.“** (F52) Neue Boulder-Logik steht in eigenen Dateien; in Pebbles und
  Cores Dateien bleibt ein Haken, der sie ruft. Wächter halten das fest: `k1_waechter.py` in der
  Firmware, `tools/boulder/vorlage_waechter.py` in der App (Abschnitt „PebbleOS 4.42 und Core 1.16“).
- Neubau auf dem aktuellen Stand von Core Devices, für App und Firmware, in den gleichen Repos. App-ID und Signatur bleiben.
- Brücke bis dahin: Boulder 0.50.0, Firmware b25, SupCycle 0.16.0, Drinktervall 1.21.0 (veröffentlicht 05.10.). Während des Umbaus läuft die Boulder-Firmware auf der Uhr.
- Datenumzug aus 0.50: einmaliger, aufgesetzter Zusatz, nach dem Release entbehrlich.
- Reihenfolge: Kraepelin-Variante → Kieselsport auf dem Workout-Dienst → Health-App → App-Teile. Der Messbau ist ausgelassen (nachholbar).
- Kein eigenes Zifferblatt in der App (TimeStyle Bedrock entfällt).

## Uhr
- Die Uhr funktioniert ohne Telefon voll.
- **Eigenes Kraepelin** als Variante an Pebbles Algorithmus-Naht (`activity_algorithm.h`, per Kconfig gewählt), mit HRV und SpO2. Kein eigener Schlafdienst mehr neben Kraepelin.
- Schritte: einzige Quelle ist die Variante.
- Nacht als Zyklen wie bei Pebble, ohne Summen-Haken. Die eigene Health-App, Boulder und Health Connect zeigen sie als einen Block mit Phasen.
- Messstufe als eigene Einstellung (1 Byte, Whitelist). Der Schieberegler in Boulder bleibt.
  - Stufe 1 = Pebble-Original: Puls nachts und tags alle 10 min, HRV aus, SpO2 aus.
  - Stufen 2–5 wie b25 (Stufe 3: 5/10/30/30, Stufe 5: 1/5/10/10 min).
- Smart Alarm streng: nie im Tiefschlaf, spätestens zur Weckzeit (Haken in `alarm.c`).
- Erkannte Spaziergänge als Sitzungen auf der Uhr. Nickerchen erkennen.
- Velo und Auto:
  - Die Uhr erkennt eine Kandidatin, Boulder bestätigt sie über eine kurze GPS-Geschwindigkeitsmessung.
  - Ohne Telefon fragt die Uhr per Vibration nach.
  - Eine erkannte Velofahrt wird als solche geloggt.
- Bei erkannter Aktivität wird die Pulsabtastung erhöht (Pebble tut das heute für Gehen und Laufen; auch für Velo).
- Coredump nach einem Absturz: Boulder holt ihn automatisch ab und legt ihn lokal ab (kein Upload). Keine Selbstabschaltung, kein doppelter Algorithmus.
- Eigene Health-App: als System-App ohne Systemrechte. Sie liest die Daten der Variante über eine kleine Schnittstelle. Nur auf emery, andere Uhren behalten Pebbles App.
- Kieselstraße bleibt System-App im eigenen Ordner.
- Zifferblatt: Original-TimeStyle aus dem Store, unverändert. Boulder tut dafür nichts Eigenes.

## Kieselsport (Weg 2)
- Komposition auf Pebbles Workout-Dienst. Pebble übernimmt Start, Pause, Stopp, 1-Hz-Puls, Erkennung aus/an und das Speichern der Sitzung.
- Kieselsport ist die einzige sichtbare Trainings-App (Pebbles Workout-App bleibt deaktiviert). Sie liefert Sportarten, Pausen mit Auto-Ende, Zonen, Runden und Bahnen, Wiederholungen, GPS, Pulskurve und die Meldung an Boulder.
- Velo-Training: als „Offen“ gestartet, die Variante zählt dabei 0 Schritte.
- Kalorien: Der Pulswert gilt. Pebbles Workout-Dienst wird dafür angepasst.
- Keine Nachfrage „Training erkannt – aufzeichnen?“ (steht auf der Ideenliste).

## Uhr-Apps
- Drinktervall und SupCycle werden eine App „Routinen“ (Wasser, Kaffee, Präparate). Sie bekommt eine Chance; mit Sorgfalt bauen.

## Arbeitsweise und Schutz
- Fitbit-Vergleich bleibt der Maßstab für den Schlaf.
- Nicht gewählt und unverändert: Signatur (Debug-Keystore), Dev-LAN-Server, Cohorts-CAs.

## App
- Upstream-Teile bleiben drin, auch Whisper. Kein Ballast-Abbau.
- Oberfläche vorerst nur Englisch.
- Alle Health-Connect-Rechte auf einmal anfragen.

## Arbeitsweise
- Jede Fassung erscheint zuerst als Vorabversion; Stable erst nach dem Test auf deinen Geräten.

- Einstellungen an zwei Orten: Health und Sport (Boulders eigene; Core-Devices-Einstellungen bleiben, wo sie sind).
- Vorerst kein Coach (Claude/MCP/lokale KI auf der Ideenliste).

## Arbeitsweise (Ergänzung)
- Ein Schritt nach dem anderen: Vor jedem Schritt 5–8 Abnahmekriterien (Entwurf von Claude, Nutzer passt an). Dann Code, Tests, Emulator, Vorabversion, Test auf den Geräten. Stable erst, wenn die Kriterien erfüllt sind; danach der nächste Schritt.

## Test auf den Geräten
- Die erste Vorabversion für die Uhr kommt erst nach Schritt 3 (Kraepelin-Variante + Kieselsport + Health-App zusammen; Nutzerwahl c). Bis dahin prüft jeder Schritt seine Kriterien im Emulator und mit Host-Tests. Kriterien, die das Gerät brauchen (7 Nächte ohne Absturz, neue Fitbit-Nächte, Coredump in Boulder), werden im gemeinsamen Gerätetest nach Schritt 3 abgenommen.
- Firmware-Vorabversionen erscheinen als GitHub-„Pre-release“. Boulder bietet sie nicht automatisch an (GithubReleases ignoriert prerelease); der Nutzer installiert die .pbz per Öffnen in Boulder (mit Rückfrage).
- V2-Firmware basiert auf upstream/main von Core Devices (Stand v4.39.0, 05.10.2026), Branch `v2` in ~/Projects/pebbleos.

## Abnahmekriterien Schritt 1 (Kraepelin-Variante), vom Nutzer bestätigt 05.10.2026
1. Pebble-Code nur berührt für: Algorithmus-Wahl (Kconfig/CMake), Messstufe-Whitelist, strenge Weckregel in alarm.c – jeweils „Boulder:“.
2. Schritte auf Pebbles Prüfkorpus gleich wie das Original; Abweichung nur bei erkannten Velo-/Autofahrten.
3. Schlaf auf den Testnächten (26.9., 29.9., 30.9., 3.10.) + ≥3 neuen gegen Fitbit mindestens so gut wie b25; Einschlafen/Aufwachen ±10 min; Schlafsumme ohne Wachminuten über Zyklen.
4. Live: im Schlaf ≥95 % der Minuten „schläft“; Smart Alarm nie im Tiefschlaf, spätestens zur Weckzeit (echter alarm.c); Nickerchen in Testfällen erkannt.
5. Velo/Auto: Kandidat bei Kieselsport-Velofahrten erkannt; ohne Telefon Nachfrage per Vibration; bestätigte Fahrt als Velo geloggt, Schritte zählen nicht (GPS-Bestätigung durch Boulder kommt mit dem App-Schritt).
6. HRV und SpO2 nach Messstufe (Stufe 1 = Pebble-Original), Puls dichter bei erkannter Aktivität, Daten über 0x4001/0x4002 (Brücken-App 0.50 empfängt sie).
7. Im Minutentakt kein Heap, kein Rechenschritt > 100 ms (QEMU); 7 Nächte ohne Absturz auf der Uhr; Coredump nach Absturz in Boulder.
8. Wechsel von b25: keine Nacht, keine Einstellung verloren, nichts doppelt.

## Entscheidungen zur Spezifikation Schritt 1 (06.10.2026)
- **F1** Wechsel von b25 mit liegender Typ-8-Sitzung: hinnehmen (einmaliger Neustart, die letzte Nacht wird wiederhergestellt). Hinweis an den Nutzer: morgens updaten, bevor eine Bewegung erkannt ist.
- **F2** HRV-Flut: Fix aus b25 übernehmen (Pebble-Stelle in hrm_manager.c, etwa 8 Zeilen).
- **F3** Eine zentrale Einstellung, die Messstufe, steuert Puls, HRV und SpO2. Auf der Uhr ersetzt sie im Einstellungsmenü Pebbles Puls- und SpO2-Intervall (Pebble-Stelle settings/health.c, wie in b25). In Boulder werden die Schalter von Core für Puls und SpO2 ausgeblendet. Die Uhr setzt Pebbles interne Werte selbst passend zur Stufe. V4 entfällt.
- **F4** Velo/Auto: Frage nach 10–12 min und am Fahrtende (spätestens nach 45 min). Ohne Antwort gilt „Nein“.
- **F5** Erkannte Velofahrt: als Pebble-Sitzung „Open“, nicht manuell.
- **F6/F9** Die Regeln aus b26 übernehmen: Wälz-Regel beim Einschlafen (40 min, 9/16) und Aufsteh-Regel (30 von 45 min). Eine Pulsbedingung trug nicht. Neue Regeln nur mit getrennten Einstell- und Prüfnächten.
- **F7** Bestätigte Velofahrt in Schritt 1: 0 aktive kcal; Pulskalorien mit Schritt 2.
- **F8** Kriterium 7: „kein Heap“ gilt für den Minutenteil; die 100 ms sind reine Rechenzeit, Warten auf Flash und Datalogging zählt getrennt (< 5 s). Das automatische Abholen des Coredumps kommt mit dem App-Schritt.
- **V1–V8** wie in der Spezifikation vorgeschlagen; V4 entfällt durch F3.
- Pebble-Stellen damit: Algorithmus-Wahl (Kconfig/CMake), Whitelist für die Messstufe, Weckregel alarm.c, HRV-Fix hrm_manager.c, Einstellungsmenü settings/health.c, in Schritt 2 Kalorien im Workout-Dienst.
- **Arbeitsweise schlank** (Nutzer, Token-Verbrauch): ein Agent pro Arbeitspaket, eine Gegenprüfung pro Schritt nur für riskante Teile, Mutation nur für Kernaussagen, kurze Berichte, weniger Rückfragen.

## Nach Schritt 1 (07.10.2026)
- **Wecker nach Neustart im Weckfenster:** Der Nutzer will diese Sicherheit („ja, diese Sicherheit möchte ich haben“).
  - Startet die Uhr später als 5 min nach Beginn des Weckfensters neu, klingelt der Wecker trotzdem: spätestens zur Weckzeit, nie im Tiefschlaf.
  - Dafür eine weitere kleine Änderung in `alarm.c`, an der schon entschiedenen Stelle der Weckregel.

## Schritt 2 – Kieselsport (07.10.2026, Spezifikation `schritt2-spezifikation.md`)
- **F1:** Kieselsport läuft in der Firmware als System-App wie in b25 und ersetzt weiterhin Pebbles offizielle Sport-App.
- **F4:** wie empfohlen. DLS 84 geht wieder in Fassung 7 mit `manual` (P4), Boulder 0.52 braucht keine Änderung.
- **F5:** wie empfohlen. Sekunden ohne Puls rechnen mit Pebbles Schrittformel, die Tagessumme der Uhr bleibt unverändert.
- **F6:** wie empfohlen. Kein Pebble-Code für das Startmenü; die App-Kachel kommt über `app_glance_db`, Quick Launch ist möglich.
- **F2: A.** Kieselsport hält das 1-Hz-Puls-Abo wie b25: KernelBG, 1 s, Sparmodus 5 s, Pause 10 s, minütlich erneuert, Ablauf 5 min. Pebble wertet über den Rundruf mit aus.
- **F3: A.** Ein kleiner Merker in Pebbles Dienst (P1, etwa 3 Zeilen) verhindert das Speichern eines verworfenen Trainings: keine Sitzung, keine Meldung.
- **V1 (geändert durch den Nutzer):** Schritte zählen im Training nur bei Sportarten mit Schritten: Laufen, Wandern, Gehen. Alle anderen Kieselsport-Arten (Velo, MTB, Kraft, Yoga, Schwimmen) zählen 0 Schritte.
- **V7:** bestätigt. Die bekannten Lücken bleiben, auch das endlose Training ohne Pause.
- **V9:** bestätigt. Die alte Store-App wird nicht ausgeblendet; erscheint sie doppelt, einmal löschen (Release-Notiz).
- **V2–V6, V8, V10–V12:** nicht kommentiert, gelten wie in der Spezifikation.

## Nach Schritt 2 (08.10.2026, Abnahme `schritt2-abnahme.md`)
1. **Wecker: A.** Die Sicherung darf `alarm.c` an drei Stellen ändern: `prv_should_smart_alarm_trigger`, `prv_record_alarm_op` und `alarm_init`, zusammen 19 Zeilen mit „Boulder:“ (c1c7b2eb0, 29753efc3). Grund: Nach einem Boot fragt Pebble die Weckregel nicht mehr. Die K1-Grenze ist entsprechend angepasst (`k1_waechter.py`, höchstens 20 Zeilen in diesen drei Funktionen).
2. **Schritte in der Pause: B.** Bei Sportarten ohne Schritte (Velo, MTB, Kraft, Yoga, Schwimmen) zählen Schritte in der Pause wieder. Gestrichen wird nur, während das Training läuft. Nur Boulder-Code.
3. **Startmenü: B, wie b25.** Kieselsport rückt im Training im Startmenü nach oben, und die Kachel zeigt die laufende Zeit („‖ h:mm:ss“ in der Pause). Dafür kommen P5 und P6 dazu (etwa 10 Zeilen Pebble-Code, mit „Boulder:“ und `CONFIG_BOULDER_KIESELSPORT`). Das ersetzt F6 = B.
4. **Kalorien (K5): ohne Vergleich mit echten Trainings umsetzen.** Die Pulsformel gilt wie in Boulder, der ±5-%-Vergleich mit echten Trainings entfällt. K5 gilt mit den Host-Tests gegen Boulders Testfälle als erfüllt. Kein adb-Export.
5. **Upstream v4.40.0: B.** Erst vor der ersten Testversion für die Uhr (nach Schritt 3) nachziehen.

## Schlaf: Vergleich mit Fitbit (08.10.2026)
- **Der Nutzer:** „Grundsätzlich fühlen sich Boulders Werte richtiger an.“ Belegt ist das beim Aufstehen am 8.10.: 06:00 (Boulder) statt 05:13 (Fitbit).
- **Folge:** Fitbit ist ein Bezug, keine Wahrheit. Eine Regel ändert Boulder nur dort, wo Puls, HRV und Bewegung die Abweichung klar stützen. Sie wird auf zwei Nächten eingestellt, auf zwei anderen geprüft und darf keine Nacht verschlechtern.
- **Auftrag:** Die V2-Schlafanalyse auf den vier Fitbit-Nächten (30.9., 2.10., 5.10., 7.10.) Minute für Minute vergleichen, systematische Fehler zeigen und gegebenenfalls eine Regel vorschlagen. Code wird erst nach Entscheid des Nutzers geändert.
- **Ergebnis des Vergleichs (08.10.):** `boulder-v2/schlafvergleich/vergleich.md` und `regel.md`.
  - **Keine neue Regel.** Keiner der drei Kandidaten besteht die Prüfung auf getrennten Nächten.
  - **Einschlafen und Aufwachen:** Boulder ist gut belegt. Fitbit beendet die Nacht zu früh (+4 bis +47 min).
  - **Wach gegen Schlaf:** Kein Widerspruch stützt Fitbit. Der strittige Abschnitt am 7.10. 02:43–03:28 sieht genauso aus wie das Wachliegen am 2.10., das beide sehen.
  - **Puls und HRV trennen ruhiges Wachliegen nicht vom Schlaf.**
  - **REM:** Boulder findet systematisch weniger, 21 % von Fitbits REM. Gründe: die feste REM-Latenz von 60 min, das Glätten, ruhiger Puls. Mit Boulders Messgrössen lässt sich das nicht entscheiden.
  - **Für später:** Nächte mit bekannter Wachzeit (Nutzer notiert, wann er nachts wach lag) bzw. HRV-Werte von 30.9./2.10., falls noch auf der Uhr.

## Schritt 3 – Health-App auf der Uhr (08.10.2026, Spezifikation `schritt3-spezifikation.md`)
- **Grundsatz (Nutzer):** Die Uhr zeigt nur Daten, die sie selbst sammelt, nichts Fremdes. Blutdruck, Gewicht, Akku-Restzeit, Trends und Urteile aus Telefondaten gibt es nur in der Boulder-App. Werte vom Telefon gehen nie an die Uhr. Einstellungen vom Telefon sind erlaubt.
  - **Präzisierung des Nutzers (08.10., widerruft die Korrektur davor):**
    - Die Uhr loggt nur die Getränke (Drinktervall) und die Präparate (SupCycle). Getränke und Präparate sind Uhrdaten.
    - **Koffein (mg, aktives Koffein, Wert zur Schlafenszeit) erscheint nur auf dem Telefon in Boulder, nie auf der Uhr.** Die Uhr rechnet es auch nicht selbst.
    - Der Ort der Getränke und Präparate auf der Uhr ist die geplante App „Routinen“ (Schritt 4).
- **F1: Karten.** In V1 (b25) luden die Karten spürbar langsam. Die Ladezeit wird deshalb beim Auslegen mitbedacht: Ursache in b25 finden, Ziele mit Messung.
- **F2: Nein zur vollen Nächteliste.** Nur HRV und Ruhepuls der letzten Nächte werden gespeichert, je Nacht mit Datum, so viele, wie Band und Urteil brauchen. Sonst keine Nachtdaten.
- **F3: Kein Tagesband auf der Uhr.** Dafür gibt es Pebbles Timeline mit den Pins.
- **F4: Ja.** Die Messstufe ist auch in der Health-App einstellbar.
- **F5: Nie.** Siehe Grundsatz; damit entfällt auch `boulder_werte`. Getränke und Präparate (geloggt auf der Uhr) sind Uhrdaten; Koffein erscheint nur in Boulder.
- **F6: Ja.** Kieselstrasse kommt vor der ersten Vorabversion nach V2.
- **F7: Hinnehmen.** Absturz der App bei einer Touch-Änderung, während sie offen ist: in die Release-Notiz, Core Devices melden.
- **Ziele einstellbar:** Schritte und Schlaf.
  - Vorschlag des Nutzers: Im Apps-Bildschirm von Boulder bekommt die Health-App einen Einstellungs-Eintrag, wie Watchfaces mit Einstellungen.
  - Die Ziele gelten auf der Uhr (Urteile, Balken) und in Boulder.
- **Kartenname:** immer „Heart“, denn auf der Uhr landen nie Körperwerte. Ersetzt V9.
- **F8–F10 (08.10., wie empfohlen):**
  - F8 A: Pulsband und Zonen von heute führt die Variante laufend mit, die App liest keinen Minutenverlauf.
  - F9 A: eigene kleine Wochen-Liste der Schritte auf der Uhr, nicht Pebbles Verlauf.
  - F10 A: Die Ziele kommen über die BlobDB Health (`boulder_ziele`, 8 B), ohne Pebble-Code. Ohne Telefon gilt der zuletzt empfangene Wert, sonst die Vorgabe.

## Schritt 3, Rest (12.10.2026, Spezifikation `schritt3-rest-spezifikation.md`)
- Der alte PC ist zurückgesetzt; gebaut wird in der Cloud (Zweig `v2`, Start-Hook `tools/boulder/sitzung-start.sh`), Notizen im Zweig `notizen`.
- **F11–F16 wie empfohlen:**
  - F11 A: Taste Oben blättert wie bei Pebble zur nächsten Karte. *Ersetzt durch F23 (14.10.): Unten vorwärts.*
  - F12 A: Today hat kein Detail, Select tut dort nichts.
  - F13 A: „Last nights“ im Night-Detail entfällt.
  - F14: Kartenfarben Today Orange, Night Indigo, Heart Folly, Activity DarkGreen, Settings DarkGray.
  - F15: Abnahmekriterien Schritt 3 wie in Abschnitt 7 der Spezifikation.
  - F16: Ladeziele L1–L6 wie rekonstruiert (L4 Auffrischen ≤ 30 ms).

## Schlaf: Nächte 7.–12.10. (13.10.2026, Analyse `naechte/analyse-2026-10-13.md`)
- **Wecker am 9. und 13.10. um 06:00** (Nutzer): Boulders Aufwachen stimmt, Fitbits Ende ist zu früh,
  wie schon am 8.10. festgehalten. Der Gang am 9.10. um 22:50 war vermutlich ein Toilettengang.
- **O2 und W30 umgesetzt** (vom Nutzer am 13.10. freigegeben; V2 `12b28664`, `4ea6fb64`):
  - O2: Ein Wegnicken vor einem Aufwach-Schub ist kein Einschlafen. 9.10.: 21:34 → 23:16 (Fitbit
    23:12).
  - W30: Wälz-Fenster 30 statt 40 Minuten. 8.10.: 22:53 → 22:35 (Fitbit 22:32). Von 27 bis 39 Minuten
    rechnen alle Nächte gleich.
  - **Zum Rahmen vom 8.10.** (zwei Nächte einstellen, zwei andere prüfen, keine Nacht schlechter):
    Beide sind nur an je einer Nacht belegt. In den übrigen sieben Nächten ändern sie nichts, also
    wird keine schlechter. Mit der Freigabe gilt das als genug.
  - **Folge:** Die Schlafrechnung von V2 ist nicht mehr bytegleich zu b26. `test_boulder_schlaf` (aus
    b26 übernommen) prüft sie jetzt, auch an den Nächten vom 7. bis 12.10. Die Vergleiche mit b26 auf
    den sechs Referenznächten bleiben grün.
- **H2 (HRV-Regel) nicht umgesetzt.** Sie hilft am 9.10., ändert aber den 5.10. ohne Beleg. Dazu passt
  der Befund vom 8.10.: Puls und HRV trennen ruhiges Wachliegen nicht vom Schlaf. Es braucht mehr
  Nächte mit Fitbit und HRV.

## Schlaf: Nacht 13.→14.10. (14.10.2026, Analyse `naechte/analyse-2026-10-14.md`)
- **Nutzer:** 03:14–04:03 geschlafen, Boulders Wachphase dort ist falsch. Aufgewacht um 05:45,
  aufgestanden um 06:00: Boulders Aufwachen (05:46) stimmt, Fitbits Ende (05:23) ist zu früh.
- **F20: A.** H2/20 in V2 übernehmen: Ein Aufwach-Schub bleibt Schlaf, wenn die HRV im Schub mindestens
  beim Median der Nacht liegt und kein Gang darin ist (unter 20 Schritten).
- **F21: A** (14.10.). Die Firmware rechnet ohne Schritte; der Gang kommt deshalb aus dem VMC wie
  beim Aufstehen (`prv_gang_ab`, H4). Das rechnet alle Nächte wie H2/20 bis auf den 12.10.
  02:25–03:12. Umgesetzt in V2 `14e34d2e`.

## Schlaf: Nacht 16.→17.10. (17.10.2026, F37, Analyse `naechte/analyse-2026-10-17.md`)
- **Nutzer:** „fitbit is the correct one with yhe slerp time. I used the phone long into the night
  ling in bed“. Fitbits Schlafzeit (Schlaf ab 01:46, Ende 06:57) stimmt; bis dahin lag der Nutzer mit
  dem Handy im Bett. Boulder (b31) zählte 22:14 bis 07:24.
- **F37: A.** In V2 kommen zwei Regeln zur Stille (VMC 0):
  - **Einschlafen:** In den 30 Minuten ab dem Einschlafen sind mindestens 20 ganz still. Geprüft nach
    den Aufwach-Schüben; ohne stille halbe Stunde bleibt das Einschlafen.
  - **Aufwachen:** Ein späterer Schlaflauf verlängert die Nacht nur, wenn er still beginnt.
- **Bei der Umsetzung nachgestellt** (Regel vom 6.10.: Einschlafen und Aufwachen höchstens zehn
  Minuten neben Fitbit; liegt eine Nacht weiter weg, werden die Regeln nachgestellt):
  - Mit der Stille allein begann der 16.10. um 01:25, 13 min vor Fitbits Bettzeit. Schiebt die Stille
    das Einschlafen, gilt ab dort die Wälzregel noch einmal: 01:39. Das gilt für jede Schwelle von 15
    bis 25 stillen Minuten.
  - Beim Aufwachen zählt nur der Anfang des Laufs: die Hälfte seiner ersten Viertelstunde. Über den
    ganzen Lauf gezählt endete ein Test (30.9. mit eingefügtem Toilettengang) 40 min zu früh, weil am
    Ende des Laufs schon die Regungen des Aufwachens liegen.
  - Die Morgenregel gilt nur, wo ein Lauf der Nacht still beginnt. Fiele das VMC nie auf 0, endete
    sonst jede Nacht am ersten Lauf.
- **Ergebnis:** 16.10. 01:39 bis 06:57 (Fitbit 01:38/01:46 bis 06:57), 302 min Schlaf (Fitbit 271).
  Alle älteren Nächte rechnen gleich. Umgesetzt in V2 `688b3097`, Vorabversion b32.

## Akku und Velo-Erkennung (20.10.2026, F38, Analyse `akku-2026-10-19.md`)
- **Nutzer (19.10.):** „Gefühlt brauchen wir seit dem neu schreiben der Anwendung mehr Akku. Die
  Fahrräder Kennung ist auch immer noch zu aggressiv. Würde das eliminieren der Funktion den Akku
  schönen?“
- **F38: A, „und Auto auch raus“** (20.10.):
  - Die Velo-Erkennung entfällt mit Vorstufe und Frage, die Auto-Erkennung ebenso.
  - Nachts misst die Uhr auf „Detailed“ wieder alle drei Minuten, wie b26 es tatsächlich tat.
- Für Velofahrten bleibt Kieselsport „Bike“ (Strasse/Gravel, MTB); es zählt keine Schritte. Ohne
  Training zählen bei einer Fahrt wieder Schritte, wie vor V2.
- Umgesetzt in V2 `4c49e827` (Entfernen), `0be700c9` (Nachtpuls) und `9caccff2` (Test),
  Vorabversion b33.

## Upstream ganz übernehmen (20.10.2026, F39)
- **Nutzer:** „Upstream?“ Die Übersicht dazu: Firmware v4.41.2 (16.10.), `main` 395 Commits nach
  v4.41.0, Cores App ohne neue Fassung. Danach: „Gibt's was das wir commiten könnten“. Die Liste
  „Rückgabe an Core Devices“ ist geprüft (`ideen.md`), noch nichts eingereicht.
- **F39: „Alles aus dem upstream übernehmen“.** V2 bekommt v4.41.2 und `main` (bis `723a1708c`).
  Das Einfrieren bis nach dem Gerätetest (F18) ist damit aufgehoben. Gemeint ist die Firmware;
  Cores App hat seit 1.15.0.1 keine neue Fassung.
- Umgesetzt in V2 `97ae1df5` (v4.41.2) und `d5cc0038` (`main`, mit den Anpassungen), Vorabversion
  b34 (`schritt3-abnahme.md`, Abschnitt 18).

## Firmware als Beta, Band gleitet (22.10.2026, F46–F48, Firmware b36, Boulder 0.58.0)
- **Nutzer:** „uh... eye candy, kannst du das band sorollen lassen? von wo es war zum jetzt kannst
  du auch gleich für das automatische firmware update sorgen? ich möchte als beta veröffentlichen“.
- **Band** (ohne Rückfrage): Jedes Öffnen der App lässt das Tagesband in 700 ms (FastOutSlowIn) von
  seiner Stellung zum Jetzt bei drei Vierteln gleiten, statt es neu zu stellen.
- **F46: Schlüssel als GitHub-Secret.** Der Seed liegt im Secret `BOULDER_SIGNATUR_SEED` von
  `boulder-firmware`. Dort signiert der Workflow „Signieren und veroeffentlichen“ dieselbe Aussage
  wie `release.sh` (b11–b25), nur mit dem Schlüssel zu `signatur.pub`.
- **F47: Beta für alle Boulder-Nutzer:** ein gewöhnliches Release mit „Beta“ im Titel, die App
  bleibt, wie sie ist.
- **F48: b36 jetzt, b37 nach dem Test** (Empfehlung).
- **Nutzer (22.10.):** „wir können weg von prereleases“. Gelesen als: keine Vorabversionen mehr,
  jede Fassung erscheint als gewöhnliches Release (die Firmware vorerst mit „Beta“ im Titel). Das
  löst „Jede Fassung erscheint zuerst als Vorabversion“ unter Arbeitsweise ab. Als Vorabversion
  markiert wird ein Release nur noch zum Zurückziehen.
- **F49: Band 600 ms wie Pebble** (Empfehlung, aus den Mockups mit vier Varianten): FastOutSlowIn wie
  Pebbles Filterzeile und App-Liste (`NotificationAppsScreen`, `LockerUtil`), statt 700 ms.
- **Nutzer:** „main darfst du vorspulen“. Boulders `main` geht nacheinander auf 0.56.0, 0.57.0 und
  0.57.1; die CI legt jedes Release an. In `boulder-firmware` blockiert die Sicherheitsprüfung den
  Push auf `main`; der Nutzer muss es dort eigens freigeben.
- **Nutzer:** „wir brauchen einen neuen signier schlüssel“. Der alte Seed ist nicht greifbar.
  - Den neuen Seed erzeugt der Nutzer selbst (`openssl rand -hex 32`). Er legt ihn ins Secret
    und in seine Sicherung, nie in den Chat.
  - Der Workflow „Schluessel zeigen“ gibt daraus nur Öffentliches aus: den Schlüssel als Hex und
    PEM und die Testsignatur für `Ed25519Test`.
  - Die App nimmt den neuen Schlüssel auf und streicht den alten, weil sie keine V1-Firmware mehr
    anbietet (F26). Das geht in 0.58.0. b36 verlangt dann Mindest-App 0.58.0.
- **Befund:** Diese Sitzung darf keine Releases anlegen, ändern oder löschen. Die GitHub-API antwortet
  mit 403: „not permitted for this session type“. Darum startet der Nutzer den Workflow von Hand.
  - Der Workflow nimmt Paket und Text aus einem Release-Commit neben `main` (`pakete/<tag>.pbz`,
    `pakete/<tag>.md`). Er prüft sie, signiert, legt das Release als Entwurf an und schaltet es frei.
  - Das Tag hält den Release-Commit.
  - App-Releases legt weiter Boulders CI an, wenn `main` eine neue Fassung bekommt.
- **Reihenfolge wie bei `release.sh`:** zuerst ein App-Release ab der Mindest-App (für b36 0.57.0),
  dann die Firmware. Der Workflow kann das nicht prüfen, weil das Repo der App privat ist.
- **Ergebnis (23.10.):** b36 ist veröffentlicht (Lauf 39907855610, Tag auf dem Release-Commit
  `2ee6e5a`). Es ist ein gewöhnliches Release mit „Beta“ im Titel und Mindest-App 0.58.0. Paket und
  Digest stimmen mit dem Release-Commit überein. Die Ed25519-Signatur ist mit dem Schlüssel aus
  0.58.0 geprüft, die Gegenprobe mit falscher Grösse schlägt fehl.
  - `signatur.pub` auf `main` trug noch den alten Schlüssel und ist nachgezogen (`05879cb`). Ein
    Release-Commit für b37 auf dieser Grundlage brach sonst ab.

## Boulder öffentlich (22./23.10.2026, Boulder 0.58.0)
- **Nutzer:** „mach das repo öffentlich. schreibe eine fancy read.me mit screenshots und viel hype
  für das projekt.“ Das README ist englisch, mit 13 Bildern (`docs/readme/`), Links in Pebbles App
  Store für Drinktervall und SupCycle und einem Abschnitt zu Kieselstrasse mit OsmAnd.
- **APK-Schlüssel:** Im Repo lag `ci/debug.keystore`, der Schlüssel jedes Boulder-APK. Öffentlich
  könnte damit jeder ein „Update“ bauen, das sich über Boulder installiert. Nutzerwahl:
  „Geschichte bereinigen“. Die CI liest ihn seit 0.58.0 aus dem Secret `BOULDER_APK_KEYSTORE`
  (sha256 der Datei `8398e03d…`). Echte Werte in Tests und Bildern bleiben (Nutzerwahl „So lassen“).
- **Befund:** Nach dem Umschreiben (git-filter-repo, Force-Push der Zweige) lieferte GitHub die
  alten Commits samt Schlüssel weiter über ihre Kennung aus. Die Activity-Ansicht nennt die alten
  Kennungen. Die Tags liessen sich mit dem GITHUB_TOKEN weder umsetzen (PATCH) noch auf alten
  Commits neu anlegen (POST). Beide Läufe brachen mit 403 „Resource not accessible by integration“
  ab: Dazwischen sind Workflow-Dateien geändert, und die Berechtigung „workflows“ kann der Token
  nicht bekommen.
- **Nutzerwahl: neues Repo** statt eines Tokens mit Workflow-Rechten und einer Bereinigung durch
  GitHub Support.
  - Das alte Repo heisst jetzt `Boulder-archiv` und bleibt privat, mit den Releases bis 0.57.1.
  - Das neue `Boulder` ist öffentlich und hat die ganze bereinigte Geschichte, damit die
    versionCode weiterzählt. Die CI baut v0.58.0 dort neu, mit versionCode 2632 statt 2629.
  - Drei Commits vom 25.09. trugen Adressen bei newtnlink.ch. Sie tragen jetzt
    `noreply@anthropic.com` bzw. die noreply-Adresse von GitHub (Nutzerwahl „Ersetzen“).
- **Vor dem Push geprüft:**
  - Kein Commit enthält `ci/debug.keystore` oder dessen Inhalt.
  - Die 12 505 Dateien der Geschichte enthalten keine privaten Schlüssel und Tokens. Gefunden
    wurden nur ein Platzhalter und zwei öffentliche Supabase-Anon-Schlüssel von Core.
  - `libcactus_engine.so` und die iOS-Bibliotheken sind bytegleich mit Cores öffentlichem Repo.
- **Regel (Boulders CLAUDE.md):** Nie aus dem Archiv oder aus einem Klon von vor dem 23.10. ins
  neue Repo pushen.
- **Ergebnis (23.10.):** v0.58.0 ist im neuen Repo veröffentlicht, aus Lauf 39904328182 auf
  `2adc74da5`. Den Push-Lauf hatte ein „[skip ci]“ in der Geschichte unterdrückt, darum wurde der
  Bau von Hand gestartet. Der erste Versuch scheiterte am Secret, das ein anderer Schlüssel war.
  - Das APK hat versionCode 2632 und dasselbe Zertifikat wie bisher (SHA-256 `212e0ca4…d035`).
    Es installiert sich als Update.
  - Im Repo stehen nur `main`, der Arbeitszweig und `v0.58.0`.
- **Firmware-Quellcode öffentlich (23.10.):** „jemand hat gesagt die firmeare ist nicht public.
  korrigiere das“. Den Vorschlag, nur `v2` getrennt zu veröffentlichen, lehnte der Nutzer ab: „nein,
  einfach auf public setzen.“ Seither ist `dysseus-pascal/PebbleOS` öffentlich, mit Standardzweig
  `v2` und allen Zweigen samt `notizen`. Die READMEs von Boulder und boulder-firmware verlinken es.
  - Vorher geprüft: alle Zweige, Tags, Pull Requests und der gelöschte Zweig
    `v2-wecker-zwischenstand`. Keine Schlüssel, Tokens, Passwörter, Koordinaten oder
    Telefonnummern.
  - Name und Adresse in `ideen.md` waren über das Signed-off-by von coredevices/PebbleOS#2314
    schon öffentlich. Die 13 echten Testnächte bleiben (Nutzerwahl „So lassen“).
  - Ab jetzt keine Zugangsdaten oder Privates in die Notizen (README dieses Zweigs, Boulders
    CLAUDE.md).

## Unterarten in Kieselsport (24.10.2026, F50–F51, Firmware b38, Boulder 0.59.0)
- **Nutzer:** „In kieselsport möchte ich unterarten im biking menu. gravel/road, MTB, indoor“, dann
  „schwimmen bekommt indoor und open water“, „open water und open water gps“ und „Mach noch custom
  und custom gps für treking ohne bestimmten sport typ“.
- **Menü der Uhr:** Running, Bike, Hiking, Strength, Yoga, Swimming, Breathing, Custom, Custom GPS.
  „Bike“ öffnet Road/Gravel, MTB und Indoor, „Swimming“ Pool, Open water und Open water GPS. Unter
  jeder Art einer Gruppe steht deren Name; die Untermenüs haben keinen Titel, beide Gruppen teilen
  eines.
- **Neue Arten, hinten angefügt** (die Zahl geht ans Telefon und in die Akte):
  - 8 Indoor: misst wie Road/Gravel, ohne Schritte und Strecke; Health Connect stationäres
    Radfahren; nie GPS.
  - 9 Open water und 10 Open water GPS: allgemeines Pulsmodell wie das Becken, ohne Bahnen; Health
    Connect Freiwasserschwimmen. Nur 10 zeichnet die Strecke auf.
  - 11 Custom und 12 Custom GPS: wie Pebbles „Workout“ (offenes Training), die Schritte zählen wie
    ohne Training; Health Connect anderes Training. Nur 12 zeichnet die Strecke auf.
  - Art 6 heisst jetzt Pool und bleibt das Schwimmen mit Bahnen.
- **F50: Pool statt Indoor** (Empfehlung) für das Becken, sonst stünde „Indoor“ zweimal da, etwa auf
  dem Startschirm und in der Trainingsliste.
- **F51: GPS als eigene Art.** Auf die Frage nach GPS bei Open water sagte der Nutzer zuerst „mach es
  optional“, dann „open water und open water gps“. Gewählt wird es auf der Uhr mit der Art, nicht mit
  einem Schalter in Boulder. Der Grund für „ohne“ als Standard: Liegt das Telefon am Ufer, zählt GPS
  Messrauschen als Strecke.
- **Seither prüft ein Test die Zahlen aller Arten** (`test_kieselsport_arten`); bis b37 tat das keiner.
- Mindest-App für b38: 0.59.0. Ältere Fassungen kennen die Arten 8 bis 12 nicht: Sie trügen sie
  als unbestimmtes Training ein und zeichneten bei den GPS-Arten keine Strecke auf.
- Umgesetzt in V2 `637c6fd5e` (`schritt3-abnahme.md`, Abschnitt 22) und Boulder `464a7466f`,
  `8dfdb76ad`; Fassung 0.59.0 in `2e7bd3993`. In der App kommen dazu: Tempo je 100 m im Freiwasser,
  kein Aufstieg beim Schwimmen (die GPS-Höhe rauscht auf dem Wasser), Custom zeigt km/h, und jedes
  „andere Training“ aus Health Connect erscheint als Custom.
- **Ergebnis (24.10.):** Boulder 0.59.0 ist veröffentlicht, aus CI-Lauf 40087614882 auf
  `2e7bd3993`: APK mit versionCode 2637 und demselben Zertifikat wie bisher (`212e0ca4…d035`). b38
  ist veröffentlicht (Tag auf dem Release-Commit `449680e` in boulder-firmware), Beta, Mindest-App
  0.59.0. Der Digest stimmt mit dem gebauten Paket überein (`a68e1cf7…`). Die Signatur ist mit dem
  Schlüssel aus 0.58.0 geprüft, die Gegenprobe mit falscher Grösse schlägt fehl. Offen: Gerätetest.

## PebbleOS 4.42 und Core 1.16, nah an der Vorlage (25.10.2026, F52, Firmware b39, Boulder 0.60.0)
- **Nutzer:** „okay, zurück zu boulder. was sagt der uostream?“ Die Übersicht: Core 1.16.0.1 (72
  Commits seit 1.15.0.1, darunter automatische Firmware-Updates und das Ende des JVM-Ziels in
  libpebble3), PebbleOS v4.42.0 und `main` danach (SpO2-Schalter in einem Satz). Dann: „ja, mach
  beides. ändere aber unseren code so das wir immer so nahe an der vorlage wie möglich bleiben.“
- **F52: nah an der Vorlage, immer** (Ergänzung zum Grundsatz vom 21.10.). Neue Boulder-Logik steht
  in eigenen Dateien; in Pebbles und Cores Dateien bleibt ein Haken, der sie ruft. Ein Wächter hält
  das fest: in der Firmware wie bisher `k1_waechter.py` (Zeilen, Hunks, Funktionen, Schalter je
  Pebble-Datei), neu darf eine eigene Datei in Pebbles Ordnern auch unter dem Schalter der Variante
  stehen. In der App neu `tools/boulder/vorlage_waechter.py` mit `vorlage.txt`: geänderte Zeilen je
  Core-Datei, genau, in der CI vor den Tests; die Zahlen sollen nur sinken.
- **Firmware b39** (V2 `2823b3ed1`): v4.42.0 und `main` bis `f7970c345`.
  - Pebble speichert die SpO2-Schalter seit v4.42 mit dem Takt in einem Satz `spo2Preferences`
    (3 Byte) und übernimmt beim ersten Start die alten Schlüssel; Boulders Stellen rufen Pebbles
    umbenannte Funktionen, der Regler hört auf den Satz wie bisher.
  - Die Zeilen „Precision“ und „SpO2 During Sleep“ stehen jetzt in `boulder_health_zeilen.c`;
    Pebbles `settings/health.c` hat statt 47 noch 19 Boulder-Zeilen (vier Haken und ein Include,
    alle unter dem Schalter der Variante), Pebbles Aufzählung und Menüs sind unverändert.
  - Die Testplattformen heissen nach dem SDK (emery statt obelix), der Startmenü-Test kennt die drei
    neuen Rufe von Pebbles Workout-App.
- **Boulder 0.60.0** (`dba1bab49`): Core 1.16.0.1. Cores Geschichte steckt seit dem Neuanlegen des
  Repos mit anderen Kennungen darin; der Merge `d7292d166` brauchte deshalb einmal ein lokales
  `git replace --graft`, seither ist `721670d28` die Merge-Basis. Datenbank 1050 (Upstream 50).
  - Cores automatische Firmware-Updates (aus wie bei Core) gehen nur über den geprüften Weg.
  - SpO2 nach Firmware: ab v4.42 der Satz mit drei Byte, die alten Schlüssel nicht mehr; darunter
    ein Byte und die beiden Schlüssel. Eine Zeile von 0.59 ohne die Schalter geht nicht an v4.42,
    weil die Uhr sie selbst übernommen hat. Cores `HealthSettings.kt`, `HealthSettingsRealDao.kt`
    und `BlobDB.kt` sind bis auf je einen Haken wieder Cores Fassung (`BoulderSpo2.kt`).
  - Boulders Datenbanktests laufen als Android-Hosttests unter Robolectric, weil Core das JVM-Ziel
    entfernt hat. Darum zählt die CI weniger Tests als bei 0.58 (2931): libpebble3 lief bisher auf
    der JVM und als Hosttest.
  - Prüfung auf dem Stand von `dba1bab49` ohne die letzte Zeile der Release-Notiz, die die Zahlen
    nennt: 2802 Tests ohne Fehler, 10 übersprungen (9 wie bei Core, dazu
    `NachtUmzugTest.echteTelefonKopie` ohne Telefonkopie); `vorlage_waechter.py` grün mit 107
    Dateien und 4018 Zeilen; 9 Mutationen in `BoulderSpo2.kt`, alle rot (eine überlebte zuerst,
    der Test für einen Satz mit nur einem Schalter kam dazu).
- **Mindest-App für b39: 0.60.0.** 0.59.0 schickt den SpO2-Satz mit einem Byte; v4.42 weist ihn ab,
  Änderungen am Telefon kämen nicht an.
- **Noch weit von der Vorlage:** In der App weichen 107 Core-Dateien ab, am meisten Cores
  Datalogging und Health-Parser (Nächte, Quarantäne, SpO2), `Health.kt`, `PlatformHealthSync.kt`
  und die Einstellungsseite. Sie auf Haken umzubauen ist eigene Arbeit; `vorlage.txt` zeigt, wo.
- **Ergebnis (25.10.):** Boulder 0.60.0 ist veröffentlicht, aus CI-Lauf 40181643156 auf
  `dba1bab49`: APK mit versionCode 5037 und demselben Zertifikat wie bisher (`212e0ca4…d035`). Die
  Zahl springt von 2638, weil Cores 2399 Commits mit ihren eigenen Kennungen dazukamen; sie steigt,
  das Update geht. Google-Klassen und Manifest-Einträge wie in 0.59.0, die URL-Hosts im Code
  unverändert. b39 ist veröffentlicht (Lauf 40182768679, Tag auf dem Release-Commit `81bd589` in
  boulder-firmware), Beta, Mindest-App 0.60.0. Der Digest stimmt mit dem gebauten Paket überein
  (`bdc8ac38…`). Die Signatur ist mit `signatur.pub` geprüft, demselben Schlüssel wie in
  `FirmwareSignatur.kt`; die Gegenprobe mit falscher Grösse schlägt fehl. Offen: Gerätetest,
  besonders die Übernahme der alten SpO2-Schlüssel beim ersten Start von b39.

## SpO2 nach Schaltern: Schlaf, Sport, Intervall (22.10.2026, F44–F45, Boulder 0.58.0, Firmware b37)
- **Nutzer:** „für spo2 möchte ich in den einstellungen checkboxen für für schlaf, sport oder
  intervall. das spart akku. für schlaf hast du vielleicht einen vorschlag wie das umgesetzt werden
  könnte. zb 1h vor der regulären bett zeit einschalten oder sowtwas“.
- **Befund:** Zwei der drei Schalter hat Pebble schon. „Blood Oxygen“ ist der Tagesplan
  (`activity.c`, `prv_spo2_subscription_update`). „Blood O2 During Activity“ misst bei Workouts und
  erkannten Aktivitäten alle 5 min einen Punkt und hält dafür den Puls bis 35 s an. Boulders
  Messstufe zog „Blood Oxygen“ bisher auf die Stufe zurück, ausschalten ging nur mit Stufe 1. Für den
  Schlaf hat Pebble nichts.
- **F44: Schlaf wie der Nachtpuls** (Empfehlung): SpO2 in der Nachtruhe, also zwischen 20 und 11 Uhr
  und solange die eben fertige Minute ruhig war (`dauerteil.c`, Spezifikation 5.2). Kein Fenster um
  eine Bettzeit. Nicht gewählt: nur bei erkanntem Schlaf (`live.c`), feste Zeit ab 1 h vor der
  üblichen Bettzeit.
- **F45: Sport wie Pebble, alle Trainings** (Empfehlung): Pebbles „Blood O2 During Activity“
  unverändert, auch in Kieselsport. Nicht gewählt: nur ruhige Trainings (Yoga, Atmen).
- **Umsetzung** (ohne Rückfrage): Die Messstufe setzt nur noch die Takte, nicht mehr „Blood Oxygen“
  an oder aus. Stufe 1 misst SpO2 stündlich, wenn ein Schalter an ist. Der Schlaf-Schalter ist der neue
  Schlüssel `boulderSpo2Schlaf` (1 Byte), in der App unter Health zwischen Pebbles SpO2-Schaltern und
  auf der Uhr unter Settings > Health.
- **Umsetzung b37** (ohne Rückfrage, Abschnitt 21 in `schritt3-abnahme.md`): Damit „Schlaf“ ohne
  „Blood Oxygen“ wirkt, erlaubt Pebbles HRM-Manager SpO2 mit „Schlaf“ wie mit dem Schalter für
  Trainings (zwei Blöcke unter dem Variantenschalter, K1 in `hrm_manager.c` von 10 auf 24 Zeilen).
  Die App zeigt Pebbles „Blood Oxygen“ samt Messrate schon seit 0.54: die Ausblendung aus F3 kam beim
  Neubau auf Core nicht mit.
- **Veröffentlicht (23.10.):** b37 ist ein Release (Lauf 39942848763, Tag auf `594f628`,
  Mindest-App 0.58.0, Signatur geprüft). Die Uhr des Nutzers lief schon mit b36, und er wollte
  den automatischen Firmware-Weg testen. Darum kam b37 als Release vor dem Test auf der Uhr
  statt als Datei zum Seitenladen; F48 („b37 nach dem Test“) ist damit überholt. Der Release-Text
  nennt es: „auf einer echten Uhr noch nicht“ geprüft. Zurückziehen ginge über „pre-release“.
- **Auf der Uhr (23.10.):** b37 kam über Boulders Update-Weg auf die Uhr des Nutzers (b36 →
  b37 mit Boulder 0.58.0). Damit ist der ganze Weg auf dem Gerät bestätigt: Release, Abruf,
  Signatur mit dem neuen Schlüssel, Mindest-App und Installation. Offen ist noch die erste
  Nacht mit „Blood Oxygen During Sleep“.

## Symbol von Kieselstrasse und Tagesband beim Öffnen (22.10.2026, Boulder 0.57.1)
- **Nutzer:** „das kieselstrassen icon... das geht besser und mehr im stil von pebbel. weiter möchte
  ich das dir timelin beim öffnen der app ¾ vergangenheit und ¼ zukunft zeigt.“
- **Symbol** (ohne Rückfrage, nach dem Grundsatz unter Grundlage): Pebbles `LOCATION`, eine Fahne auf
  einem Stück Boden (`Pebble_25x25_Location.svg`). Dasselbe zeigt die Uhr im Launcher für
  Kieselstrasse (`RESOURCE_ID_LOCATION_TINY`). Gezeichnet wie die anderen Symbole der Liste: 114 px,
  weiss mit 6 px schwarzer Kontur, Grund `#00AA55`. Nicht gewählt: Pebbles grosse Fassung, eine Karte
  mit Fahne (`Pebble_80x80_Location.svg`).
- **Tagesband:** Das Jetzt bei drei Vierteln gab es schon (`JETZT_ANTEIL`), aber nur beim ersten
  Aufbau. Kam die App aus dem Hintergrund, stand das Band, wo es zuletzt war. Jetzt stellt jedes
  Öffnen der App (ON_START der Activity nach ihrem ON_STOP) das Band neu; dazwischen bleibt es, wo man
  es hinwischt.

## Alle Gesundheitsdaten und die Atemübung (22.10.2026, F41–F43, Boulder 0.57.0, Firmware b36)
- **Nutzer:** „ich möchte das sämtliche gesundheitsdaten für den mcp zurverfügung stehen. ich meine
  auch daten die von healthconnect kommen. Dazu möchte ich eine einstellbare pranayama atemübung in
  kieselsport“.
- **Daten** (ohne Rückfrage): Der Spiegel schickt jeden Satz aus Health Connect, aus jeder App (alle
  41 Arten, ohne Routen), dazu die Vorgeschichte (Kiesel-Helper, ältere Sicherungen) und das SpO2
  der Uhr. Wie, steht in `ki-mcp-konzept.md`, Abschnitt 9.
- **F41: Eigener Rhythmus** (Empfehlung): Einatmen, Halten, Ausatmen und leer Halten in Sekunden,
  dazu die Runden. Keine festen Muster zur Auswahl.
- **F42** (Nutzer): „In boulder ja, aber so wie watchfaces die einstellungen haben.“ Eingestellt
  wird in Boulder, die Uhr stellt nichts ein. Kieselsport hat in der Liste der Apps „Settings“ wie
  ein Watchface mit Einstellungsseite (`LockerAppScreen`); es öffnet die Seite Sport.
- **F43: Als Training speichern** (Empfehlung): wie die anderen Arten. In Health Connect als
  geführtes Atmen (`EXERCISE_TYPE_GUIDED_BREATHING`), mit Puls und HRV wie Yoga, die Runden und der
  Rhythmus in der Notiz.
- **Nähe zu Pebble:** Pebble hat keine Atemübung. Boulder baut sie aus Pebbles Teilen: eine Art von
  Kieselsport auf Pebbles Workout-Dienst, der Kreis mit Pebbles Kurve (`animation_timing_curve`,
  ease-in-out), das Brummen als `VibePattern`.
- **Umgesetzt** in Boulder 0.57.0 (`cfdb8b339`) und V2 `4103a43fe` bis `214a624cd`, Vorabversion b36
  (`schritt3-abnahme.md`, Abschnitt 20). Selbst entschieden: Zonenwechsel brummen beim Atmen nicht;
  das Ende wartet höchstens 5 s, bis der Dienst die ganze Zeit gezählt hat; ein neuer Rhythmus
  während einer laufenden Übung gilt erst beim nächsten Öffnen der App.

## Health: Karten gleiten wie bei Pebble (21.10.2026, F40)
- **Nutzer:** „Können wir in health vertikal scrollen?“ Heute springen Oben/Unten (und Wischen, das
  Pebbles Touch-Brücke als Taste meldet) ohne Übergang zur nächsten Karte; die Details scrollen schon
  mit Tasten und Finger.
- **F40: A**, „Wir bleiben mit allem was wir machen so nahe an pebble wie es geht“ (Grundsatz, siehe
  Grundlage): Die Karten gleiten beim Wechsel vertikal wie in Pebbles eigener Health-App
  (`fw/apps/system/health/card_view.c`). Nicht gewählt: B, alle Karten auf einer Seite, die dem
  Finger folgt.
- **Umgesetzt** in V2 `685b50fe9` (b35):
  - Je Karte eine Ebene wie bei Pebble. Kurve und Dauer sind Pebbles: moook mit einem Bild in der
    Mitte, 8 Bilder zu 33 ms = 264 ms. Die Richtung folgt der Taste, Oben von oben, Unten von unten,
    auch beim Wischen. Eine Taste mitten im Gleiten bricht es ab wie bei Pebble.
  - Beim Federn am Ende (bis 4 px) trägt das Fenster die Farbe, die an die neue Karte grenzt: oben
    ihren Kopf, unten den weissen Inhalt. Pebble färbt dafür das Fenster in der Farbe der Karte.
  - **Details wieder mit Pebbles Übergang** (aus der Health-App, `activity_summary_card.c:196`). AP8
    hatte ihn für L2 weggelassen; das war eine Massnahme aus der Messung, kein Entscheid des Nutzers.
    Mit dem Grundsatz gilt Pebbles Verhalten.
  - **L2** begrenzt jetzt den Anteil der App (`a9133d56e`), weiter mit 30 ms: die Zeit von der
    Taste bis zum ersten Bild des Gleitens und das Zeichnen des Details. Wann das Neue zu sehen
    ist, bestimmt Pebble:
    - Die neue Karte kommt ein Bild nach dem Beginn herein (33 ms).
    - Pebbles Übergang zum Detail zeichnet sein erstes Bild erst, wenn das Detail hereinkommt, in
      QEMU nach 40–60 ms (wie schon in AP8).
    - Diese Zeiten stehen ohne Grenze in `messung.json`. Die alte Grenze für die ganze Zeit hielte
      nur ein Wechsel ohne Pebbles Animation.
    - Das erste Gleiten und das erste Detail nach dem Boot zählen nicht: QEMU übersetzt dabei
      Pebbles Animationscode zum ersten Mal (wie F17).
    - `qemu_health.py` prüft dazu, dass jede Karte nach 264–330 ms ruht und schon im Gleiten zu
      sehen ist.
  - Pebbles Kurve steht nicht im SDK: drei Helfer ohne Systemrechte mehr in `pruefe_ohne_rechte.py`
    (Pebbles Applib nutzt sie selbst in der App-Task), die Probe der Bausteine benutzt sie.
  - Nebenbefund `967da424b`: QEMU zählt 1000 Ticks je Sekunde, `qemu_health.py` rechnete mit 1024.
    Jede QEMU-Zeit war 2,4 % zu klein; kein Urteil kippt.
  - Nebenbefund `e044bda27`: Die Probe der Bausteine endete seit b34 beim Schliessen an der MPU.
    Seit Upstream `main` schreibt `menu_layer_destroy` für eine App mit System-ID Kernel-RAM
    (seitlich laufender Menütext). Die Health-App benutzt keine MenuLayer, Kieselsport läuft mit
    Systemrechten; betroffen war nur die Probe. Sie kommt jetzt ohne MenuLayer aus, und
    `pruefe_ohne_rechte.py` meldet `menu_layer_destroy`.
  - Noch nicht wie Pebble: Pebbles Karten zeigen oben und unten Pfeile (ContentIndicator) und rechts
    den Punkt für Select; Boulders Karten haben dafür die fünf Punkte im Kopf. Frage an den Nutzer.

## Vor der ersten Vorabversion (13.10.2026, Abnahme `schritt3-abnahme.md`, Abschnitte 7 und 8)
- **Upstream v4.40.0** nachgezogen (`0510648c`), wie am 8.10. entschieden.
- **F17: B.** Den ersten Start nach dem Boot misst `qemu_health.py` in einem zweiten Boot unter
  `-icount shift=3`. In Echtzeit zählen die übrigen Starts wie bisher. Grenze für beide: 50 ms.
- **F18: A.** Upstream v4.41.0 jetzt nachziehen. Danach bleibt der Upstream-Stand **eingefroren bis
  nach dem Gerätetest**: Der Gerätetest prüft den Code, der bleibt.

## Erste Vorabversion V2 (14.10.2026)
- **V2 jetzt veröffentlichen** (Nutzer): b27 mit der HRV-Regel, Kieselstrasse kommt danach (F6
  geändert: nicht mehr vor der ersten Vorabversion).
- **Kein Vorher-Video von b26** (Punkt 5 gestrichen): Die Ladezeiten auf der Uhr beurteilt der Nutzer
  selbst; diese Einschätzung genügt für Kriterium 6.

## Kieselstrasse in V2 (14.10.2026, F6)
- **Port aus b26** (Vorabversion b28): eigene System-App unter `CONFIG_BOULDER_KIESELSTRASSE`. Der
  Schalter hängt an der Variante, die Boards ohne sie bleiben bei Pebble. Registry-ID -104 (b26: -103;
  in V2 trägt -103 Kieselsport). Kennung, Schlüssel und Persist wie die Store-App.
- **Store-App löschen löscht auch den Speicher der eingebauten App** (Befund beim Port): V2 entfernt
  beim Löschen einer Store-App die Persist-Datei ihrer Kennung, auch wenn Kieselsport oder
  Kieselstrasse sie mitbenutzen (Maxpuls, Beckenlänge, wartende Trainings; der letzte
  Navigationsstand). b26 verhinderte das. **Nutzer: nichts ändern** – nur eigene Daten, nichts
  veröffentlicht. Die Release-Notiz rät weiter, eine doppelte Store-App einmal zu löschen (V9).
- **Auf der Uhr getestet** (Nutzer, 14.10.): Kieselstrasse läuft mit b28.

## Upstream v4.41.1 vor dem Gerätetest (14.10.2026, F22)
- **F22: A.** Upstreams Patch-Fassung v4.41.1 (heute erschienen) kommt vor dem Gerätetest in V2,
  als b29. Sie korrigiert die Worker von Apps: In v4.41.0 (und damit in b27/b28) startet kein Worker,
  `WorkerState` ist 648 statt höchstens 640 Byte; ein fehlgeschlagener Start kann danach assertieren.
  `main` bleibt eingefroren (F18): 36 weitere Commits, darunter Treiber-Umbauten (I2C, GPIO,
  nPM1300 auf dem neuen Gerätemodell).

## Health-App: Unten blättert vorwärts (14.10.2026, F23)
- **F23** (Nutzer, auf der Uhr mit b29): „In Health sind die Buttons verkehrt. Nach unten soll
  vorwärts sein.“ Ersetzt F11 A (Oben zur nächsten Karte wie bei Pebble, nach Quick Launch Unten
  gespiegelt). Jetzt: Unten blättert zur nächsten Karte, Oben zurück, nach jedem Start. Kreis und
  erste Karte bleiben wie bei Pebble; nach Quick Launch führt Oben auf Today zum Zifferblatt.
  Umgesetzt in V2 `a3652f881`, Vorabversion b30.

## Velo-Frage beim Gehen (16.10.2026, F32, F33)
- **Meldung** (Nutzer, mit b30): „while walking around the watch asks me if i am biking. the detection
  seems a bit agressiv.“ Nachgestellt an den Wachminuten der echten Nächte: Armarbeit zu Hause sieht
  für Schritte, VMC und Lage aus wie Hände am Lenker; der Puls lag dort im Median 23 über dem
  Ruhepuls, die Fensterschwelle war +20 und entfiel ohne Pulswerte ganz.
- **F32** (Nutzer: „wir machen beides“): Sofortmassnahme in der Firmware. Velo nur mit mindestens drei
  Pulswerten im Fenster und einem Median ab 30 über dem Ruhepuls. Ruhige Fahrten unter +30 fragen nicht
  mehr. Umgesetzt in V2 `b3d0828de`, Vorabversion b31.
- **F33** (Nutzer): Minutenexport im Debug-Teil der App, um die Schwellen an echten Fahrten einzustellen.
  Der Nutzer will zuerst einen Vorschlag, mit dem sich gezielt eine bestimmte Dauer exportieren lässt.
  Vorschlag vom 16.10.: Beginn (Datum, Uhrzeit) und Dauer (30 min bis 24 h oder Endzeit), Schnellwahl
  aus den letzten Kieselsport-Trainings (±15 min), Vorschauzeile, höchstens 7 Tage; CSV im Format des
  Nachtexports mit Ruhepuls, Firmware und Markierungen (Trainings, bestätigte Fahrten, Schlaf); als
  Datei teilen. Gebaut wird erst nach dem OK. **OK des Nutzers (16.10.): „passt so“.** Umgesetzt in
  Boulder `eb84e7d51`, Fassung 0.54.2 (`310e2c869`, versionCode 2578), APK zum Seitenladen; `main`
  nach dem Gerätetest.

## KI-Einbindung über MCP (16.10.2026, F34–F36, Konzept `ki-mcp-konzept.md`)
- **Auftrag** (Nutzer): „sieh dir den upstream an und überlege dir die ki einbindung über einen mcp“.
- **F34** (Nutzer): „wir machen eine kleine server applikation. sagen wir etwas das in podman läuft“.
  Dazu die Daten als Spiegel auf dem Server (Empfehlung angenommen) und der Zugang über einen
  Cloudflare Tunnel („wir richten einen cloudflare tunnel ein“).
- **Anmeldung** (ohne Rückfrage, 16.10.): Feste Request-Header gibt es bei Claudes Connectors nur
  als Beta für einige Organisationen (Anthropic-Doku „Authentication for connectors“). Damit es mit
  jedem Konto geht, kann der Server OAuth 2.1 mit Registrierung, PKCE und Erneuerung. Freigegeben
  wird im Browser mit dem MCP-Token. Der feste Header bleibt für Claude Code.
- **F35** (nicht beantwortet, Vorschlag umgesetzt): Tage, Nächte und Trainings sind an, die
  Minutendaten aus. Jede Art lässt sich in Boulder schalten.
- **F36** entfällt: Claude läuft in der Claude-App des Nutzers, Boulder braucht weder einen
  API-Schlüssel noch zahlt es je Token.
- **Abweichungen vom Konzept (§7):**
  - Ins Protokoll kommen nur Fehler und deren Ende, nicht jedes Senden: bei einem Senden alle
    15 Minuten wären die 100 Zeilen nach einem Tag voll. Der Stand steht auf der Einstellungsseite.
  - TLS liefert der Tunnel, nicht Caddy.
  - Tage gehen nur mit ganz gelesener Akte hinaus. Im Hintergrund ohne Leserecht fehlten Wasser
    und Koffein, und der Server hätte die Tage ohne sie ersetzt.

## Schritt „App-Teile“: Boulder auf Core Devices (14.10.2026, Entwurf `app-spezifikation.md`)
- **Auftrag** (Nutzer): Boulder auf die App von Core Devices aufbauen, „so dass wir nicht einen Fork
  forken“.
- **F24** (Vorschlag, nicht gefragt): Basis Core 1.15.0.1 (`ec2156cc`, 08.10.), danach eingefroren bis
  zum Gerätetest der neuen App.
- **F25: A.** Google-Teile von Core bleiben drin, stillgelegt: Attrappe für `google-services.json`,
  keine Tokens, Sammeln aus; ein Kriterium listet die Netzwerkziele.
- **F26: A.** Die neue App unterstützt nur noch die Firmware V2 (b27 und neuer). TimeStyle Bedrock
  fällt weg; wer b26 braucht, bleibt bei 0.53.
- **F27: A.** PKJS-Rechte nach Cores Modell. Gravels Freigaben aus 0.53 gehen dabei verloren (Core
  löscht `LockerAppPermission` in Fassung 48).
- **F28: Cactus behalten** (gegen die Empfehlung, Cactus wegzulassen). Offline-Diktat wie bei Core; die
  Lizenzfrage für ein öffentliches APK bleibt offen.
- **F29: Ring/Index lassen wie bei Core** (14.10., Empfehlung). Kein Eingriff ins Modul `experimental`:
  Cores Teilen-Ziele „Index“ sind ab Werk aus und gehen erst mit gekoppeltem Ring an, die Ring-Rechte
  werden ohne Ring nie erfragt. Damit bleiben `kotlin-reflect` (über das MCP-SDK) und die Ring-Bilder
  im APK; Boulders Ausschlüsse dafür aus 0.53 kommen nicht mit.
- **F30: Fassung 0.54.0** (14.10., Empfehlung) für die erste App auf Cores Grundlage.
- **F31: Gerätetest per APK** (14.10., Empfehlung). Der Release-Build kommt zum Seitenladen; `main`
  bleibt auf 0.53, bis der Gerätetest bestanden ist, dann `main` und Release `v0.54.0`.
- **Vorgabe** (Nutzer, 14.10.): „wir werden nichts an core devices senden das unsere app oder
  firmware betrifft. alles was auf bug reports oder crash logs hinweist wird ausgeblendet“. Gilt auch
  für Cores Ring/Index-Teil (F29 bleibt sonst): dort fallen „File Bug Report“ und „Diagnostics“ weg.
  Gravels lokales „Export logs“ aus 0.53 kommt nicht zurück.

## Boulder-App (13.10.2026, berichtigt 14.10.)
- **Die App baut nicht auf Gravel auf, sondern auf dem Original von Core Devices**
  ([coredevices/mobileapp](https://github.com/coredevices/mobileapp)). Das ist der Entscheid vom
  05.10. unter „Grundlage“: Neubau auf dem aktuellen Stand von Core Devices, für App und Firmware.
  Der Neubau der App ist dort der letzte Schritt („App-Teile“). Bis dahin läuft das bisherige
  Boulder als Brücke, und es kommen keine Gravel-Fassungen mehr hinein.
- Der Punkt „Gravel 0.3.4“ auf der Liste bis zur Vorabversion war falsch; er ist aus Boulder 0.53
  wieder entfernt (`aecd3350`). Für V2 war 0.3.4 nicht nötig: Boulder liest V2s Minutendaten
  (Fassung 14) seit b25.
- Mein Eintrag in Boulders CLAUDE.md vom 13.10. sagt nur „nicht mehr auf Gravel“, nicht wohin.
  Ihn zu berichtigen ist offen.
- **F19: A** (14.10.). Boulder 0.53 erscheint als Brücke für den Gerätetest, nur mit Boulders eigenen
  Teilen (Goals, V2-Erkennung). Der Neubau der App auf dem Original von Core Devices folgt wie am
  05.10. geplant als Schritt „App-Teile“. `main` steht auf `aecd3350`; die CI legt `v0.53.0` an.
