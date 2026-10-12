# Boulder V2 – Ideenliste

Später prüfen; nicht Teil der aktuellen Planung.

- **„Training erkannt – aufzeichnen?“ in Kieselsport übernehmen** (05.10.2026, Punkt 13): Erkennt die Uhr einen Spaziergang, bietet sie an, ihn als Kieselsport-Training ab dessen Beginn weiterzuführen. Kleiner Haken, der Pebbles Meldung auf Kieselsport umleitet.
- **Pull Requests an Pebble/Core Devices** (Weg 3): Allgemein Nützliches anbieten, etwa mehr Sportarten, Pulszonen und Auto-Ende. Öffentlich, nur mit Zustimmung des Nutzers.
- **Schlafwert 0–100 und HRV-Status wie bei Garmin** (7 Nächte gegen eine Basis über 3 Wochen).
- **Dichtere Nachtmessung für bessere Phasen** (G2: 5 von 10 min, G1: durchgehend): Erst den Akku messen. Oura gewann mit HRV deutlich an Genauigkeit.
- **Messbau** (3 Nächte: längster Rechenschritt, Warteschlange, freier Speicher, RR-Lücken), nachholbar.
- **Coach** (05.10.2026 vorerst nicht): Coach-Paket zum Teilen in Claude; MCP auf dem PC (liest Sicherung) oder auf dem Telefon (WLAN); eigener claude.ai-Konnektor über eigenen Server; Chat in Boulder über die Claude-API; Claudes eigene Health-Connect-Anbindung, sobald sie in der Schweiz verfügbar ist.
- **Lokale KI auf dem Pixel 8** (Llama 3.2 3B / Gemma 2B, 4 Bit; llama.cpp neben whisper.cpp oder MediaPipe/LiteRT): taugt zum Erklären vorgerechneter Werte, als Datenanalyst schwach; NPU für fremde Apps kaum nutzbar; 1,5–2,5 GB Modell, nur auf Abruf laden.
- **Langes stilles Sitzen nach dem Aufstehen** (b26-Grenze ~38 min, 06.10.2026): In V2 den beendeten Wecker und das Licht (hell in der Küche, dunkel im Bett) als Signal fürs endgültige Aufstehen nutzen.

## Rückgabe an Core Devices (gemerkt 07.10.2026, nach V2 Schritt 2 angehen)
Vorgehen:
1. Jeden Punkt gegen das aktuelle Upstream prüfen.
2. Was dort noch besteht, wird ein einzelner kleiner PR mit Test, englisch und ohne Boulder-Bezug.
3. Absenden erst nach Freigabe des Nutzers, je PR einzeln.

Kandidaten:
1. **HRV-Flut** in `fw/services/hrm/hrm_manager.c`, unser Fix F2 auf v2. Upstream hat die Stelle seit Basis a2120e2ae nicht geändert. **EINGEREICHT am 07.10.2026:** https://github.com/coredevices/PebbleOS/pull/2314 (aus der Kopie dysseus-pascal/PebbleOS-upstream, Zweig hrm-hrv-broadcast, Signed-off-by Oliver Lienhard <pebble@newtnlink.ch>).
2. **Kraepelin, Verlauf mitten in der Minute** (D7): Das Lesen setzt die Bewegung zurück und verbraucht SpO2. Am Original prüfen.
3. **Kraepelin, Überlauf** (T2.4): sättigen bei 65 535 statt überlaufen. Am Original prüfen.
4. **`workout_service.c`:** Der Timer für `duration_s` lässt Ticks aus (Spezifikation Schritt 2, G7, :280-301). Prüfen, ob es ein echter Fehler ist.
5. **`settings_file.c:513`:** Die Platzprüfung verlangt auch beim Überschreiben den vollen Platz (Gegenprüfung Schritt 1, B1). Prüfen, ob das Absicht ist.
6. **libpebble3 (Core-App):** ACK erst nach dem Speichern der Uhr-Daten plus Quarantäne, wie Boulder 0.50. Am Original prüfen.

## Hypnogramm im Schlaf-Pin der Timeline (Idee, 08.10.2026)
- **Idee:** Pebbles Health-Insights legen am Morgen schon einen Schlaf-Pin an, mit Titel und Untertitel, ohne Grafik (`fw/services/timeline/health_layout.c`).
- **Erweiterung:** Die Pin-Vorlage zeichnet ein kleines Hypnogramm (vier Bahnen wie in Boulder, Dauer, Zeiten). Die Phasen der Nacht (etwa 100–200 B) reisen im Pin mit, damit auch ältere Pins ihre Grafik behalten.
- **Aufwand:** Eingriff in Pebbles Timeline-Code (etwa 40–80 Zeilen, besser als eigene Datei mit einer Zeile Anmeldung, nur emery).
- **Offen:** Ersetzt der Pin Pebbles Schlaf-Pin oder kommen beide? Mini-Grafik schon in der Liste oder erst geöffnet?
- **Stand:** Vom Nutzer auf die Ideenliste gesetzt, weil Pebble schon einen Pin mitbringt. Nicht in Schritt 3.
