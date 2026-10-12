export const meta = {
  name: 'v2-schritt3-ueberarbeiten',
  description: 'Schritt-3-Spezifikation nach den Entscheiden vom 08.10. überarbeiten: Ladezeit der Karten, schlanke HRV/Ruhepuls-Liste, kein Tagesband, Ziele einstellbar über Boulder; gegenprüfen',
  phases: [
    { title: 'Überarbeiten', detail: 'Ladezeit-Ursache b25, Spezifikation nachführen' },
    { title: 'Gegenprüfung', detail: 'gegen Code und Entscheide' },
  ],
}
const ORT = '/home/dysseus/Documents/Claude/boulder-v2'
const S = '/tmp/claude-1000/-home-dysseus-Documents-Claude/a0d87cde-04f6-43b4-bc17-a2150aed9501/scratchpad/s3-bestand'
const GRUND = `Boulder V2 Schritt 3 (eigene Health-App auf der Uhr). Massgebend: ${ORT}/entscheidungen.md, Abschnitt "Schritt 3 – Health-App auf der Uhr (08.10.2026)" (Grundsatz: Uhr zeigt nur selbst gesammelte Daten; F1–F7; Ziele einstellbar; Karte immer "Heart"). Spezifikation: ${ORT}/schritt3-spezifikation.md, Bestand: ${ORT}/schritt3-bestand-pebble-health.md, ${ORT}/schritt3-bestand-daten.md. Code lesen: Firmware ~/Projects/pebbleos (origin/v2 836b406e7 bzw. neuester origin/v2; main = b26 mit der b25-App src/fw/apps/system/boulder_health), Boulder ~/Projects/Boulder (0.52.0). Lesen per git archive nach ${S}. NICHTS in Repos ändern oder committen. Deutsch, knapp, Datei:Zeile-Belege.`

phase('Überarbeiten')
const neu = await agent(`${GRUND}

Überarbeite ${ORT}/schritt3-spezifikation.md vollständig nach den Entscheiden (Abschnitt 10 durch die Entscheide ersetzen und ALLE betroffenen Stellen nachführen, nicht nur anhängen):
1. LADEZEIT (F1): Finde am Code von main (b25-App boulder_health und ihre Datenwege) heraus, WARUM die Karten in V1 langsam luden (z. B. ganzes 128-KiB-Minutenfile lesen, synchrone Syscalls, Neuberechnung je Kartenwechsel, Rendern) – mit Beleg, wenn möglich gemessen (QEMU-Prüfbau oder Host-Messung der betroffenen Funktionen). Lege daraus eine Ladestrategie für die neue App fest: was beim Start sofort steht (Gerüst, Titel, zuletzt bekannte Werte), was im Hintergrund kommt, was zwischengespeichert wird (App-Heap, je App-Start einmal), was nur im Detail gelesen wird; messbare Ziele (z. B. Karte sichtbar < 100 ms nach Taste, Werte < 500 ms, Kartenwechsel ohne erneutes Lesen) und wie sie in QEMU/Host gemessen werden (Kriterium + Test).
2. F2: statt Nächteliste nur HRV und Ruhepuls der letzten Nächte mit Datum, so viele wie HRV-Band (30 Tage) und Urteile brauchen; Struktur, Grösse, Speicherort, Wechsel b26→V2.
3. F3: kein Tagesband; Today-Detail streichen oder sinnvoll ersetzen (Vorschlag mit Begründung, ohne neue Daten).
4. F5/Grundsatz: alles streichen, was Telefonwerte auf der Uhr vorsieht (boulder_werte usw.); prüfen, dass jede Zahl auf jeder Karte aus Uhrdaten stammt.
5. ZIELE einstellbar (Schritte, Schlaf): Speicherort auf der Uhr, Weg vom Telefon (Boulder: Einstellungs-Eintrag für die Health-App im Apps-Bildschirm wie bei Watchfaces mit Einstellungen – am Boulder-Code prüfen, wie Watchface-Einstellungen dort heute laufen, und welchen Kanal zur Uhr man nutzt: settings_blob_db-Whitelist (Platz?), healthdb, AppMessage an die System-App?), Pebble-Eingriffe dafür, Boulder-Gegenstück (gehört in die Boulder-Fassung vor dem Gerätetest), Vorgabewerte (8 h, 10 000), Gültigkeit auch in Boulder (Urteile/Ziellinien).
6. Karte "Heart" fest (V9 ersetzt), F4 Messstufe in der App, F6 Kieselstraße als eigenes Paket vor der Vorabversion, F7 hinnehmen (Release-Notiz, Meldung an Core Devices).
Arbeitspakete, Kriterien, Pebble-Eingriffe und Abschnitt "Nach Schritt 3" entsprechend nachführen. Neue echte Entscheidungen (falls nötig) als kurze F-Liste mit Empfehlung am Ende. Gib eine Kurzfassung zurück (max. 15 Zeilen): Ursache der Langsamkeit, Ladestrategie und Ziele, Weg der Ziele vom Telefon, neue offene Fragen.`, { label: 'ueberarbeiten', phase: 'Überarbeiten' })

phase('Gegenprüfung')
const pruefung = await agent(`${GRUND}

Prüfe die überarbeitete ${ORT}/schritt3-spezifikation.md adversarial: Ist die Ursache der Langsamkeit belegt (nicht vermutet)? Halten die Ladeziele mit den vorgesehenen Datenwegen (Syscall-Stapel 2 KiB, KernelBG-Aufträge, Dateigrössen)? Stammt jede angezeigte Zahl aus Uhrdaten (Grundsatz)? Ist der Weg der Ziele vom Telefon machbar und vollständig (Whitelist-Platz, Synchronisation, Fallback ohne Telefon)? Stimmen Pebble-Eingriffe und Zeilengrenzen? Korrigiere direkt in der Datei und hänge einen Abschnitt "Gegenprüfung 2" an. Gib die Korrekturen zurück.`, { label: 'gegenpruefung', phase: 'Gegenprüfung', effort: 'high' })
return { neu, pruefung }
