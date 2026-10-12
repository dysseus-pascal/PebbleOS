# Boulder V2 – Entscheidungen

Stand: 05.10.2026. Quelle: Diskussion mit dem Nutzer. Unterlagen zu den Schlaf-/Schrittvorschlägen in diesem Ordner.

## Grundlage
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
