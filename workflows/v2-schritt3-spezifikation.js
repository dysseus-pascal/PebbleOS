export const meta = {
  name: 'v2-schritt3-spezifikation',
  description: 'V2 Schritt 3 auslegen: eigene Health-App auf der Uhr – Bestand erheben, Spezifikation mit offenen Fragen schreiben, gegenprüfen (nur Dokumente)',
  phases: [
    { title: 'Bestand', detail: 'Pebbles Health-App und Datenquellen der Variante parallel erheben' },
    { title: 'Spezifikation', detail: 'schritt3-spezifikation.md' },
    { title: 'Gegenprüfung', detail: 'Spezifikation gegen den Code prüfen' },
  ],
}
const ORT = '/home/dysseus/Documents/Claude/boulder-v2'
const S = '/tmp/claude-1000/-home-dysseus-Documents-Claude/a0d87cde-04f6-43b4-bc17-a2150aed9501/scratchpad/s3-bestand'
const GRUND = `Boulder V2, Firmware ~/Projects/pebbleos, Zweig origin/v2 (Stand 836b406e7; lesen per git archive/worktree nach ${S}), App Boulder ~/Projects/Boulder (main, 0.52.0). Schritt 1 (eigenes Kraepelin) und 2 (Kieselsport auf dem Workout-Dienst) sind fertig: ${ORT}/schritt1-abnahme.md, ${ORT}/schritt2-abnahme.md; Spezifikationen als Vorbild für Aufbau und Strenge: ${ORT}/schritt1-spezifikation.md, ${ORT}/schritt2-spezifikation.md. Entscheidungen: ${ORT}/entscheidungen.md – für Schritt 3 massgebend: "Eigene Health-App: als System-App ohne Systemrechte. Sie liest die Daten der Variante über eine kleine Schnittstelle. Nur auf emery, andere Uhren behalten Pebbles App." Dazu: Nacht als Zyklen, ein Block mit Phasen; Messstufe als zentrale Einstellung (F3, ersetzt Pebbles Puls/SpO2-Menü in settings/health.c); Einstellungen an zwei Orten (Health und Sport); UI nur Englisch; keine Textwände (nur Zahlen, Urteile, kurze Labels); Boulders Design (Health-Tab, Tagesband, Heart & Body, Nachtansicht) als Vorbild für Inhalte. Ideenliste ${ORT}/ideen.md (Tagesband auf der Uhr als Idee). Uhr: Pebble Time 2 (obelix/emery, 200×228, Farbe, Touch?) – am Code prüfen.
NICHTS in den Repos ändern, nichts committen. Deutsch, knapp, mit Datei:Zeile-Belegen.`
const BESTAND = { type: 'object', properties: { datei: { type: 'string' }, zusammenfassung: { type: 'string' } }, required: ['datei', 'zusammenfassung'] }

phase('Bestand')
const [pebble, daten] = await parallel([
  () => agent(`${GRUND}

Erhebe PEBBLES HEALTH-APP auf origin/v2: Ordner und Dateien, Bildschirme und Navigation (Kartenansicht, Detail), welche Daten sie wie liest (health_service, activity-APIs, Sitzungen, Schlaf, Puls, HRV/SpO2), Abhängigkeiten zu Kraepelin/Variante (was zeigt sie heute mit der Variante falsch oder gar nicht, z. B. Zyklen, HRV, SpO2, Messstufe), wie eine System-App registriert/ersetzt/ausgeblendet wird (Registry, Startmenü, Quick Launch, Shortcut vom Zifferblatt, Health-Insights/Meldungen, Timeline-Pins), was eine System-App OHNE Systemrechte darf (Syscalls, Speicher, Heap/Stack, Grafik, Hintergrund), Bildschirmgrössen/Eingabe von obelix/emery (Display, Touch, Tasten), Bauhaken für "nur emery". Schreibe ${ORT}/schritt3-bestand-pebble-health.md und gib Pfad + Kurzfassung zurück.`, { label: 'bestand-pebble-health', phase: 'Bestand', schema: BESTAND }),
  () => agent(`${GRUND}

Erhebe die DATENQUELLEN für eine eigene Health-App auf der Uhr: was die Variante (Schritt 1) und Kieselsport (Schritt 2) auf der Uhr bereithalten (Nächte/Zyklen/Phasen, HRV-Fenster, SpO2-Ring, Ruhepuls, Messstufe, Nickerchen, Spaziergänge, Velo/Auto, Trainings, Kalorien, Schritte je Minute/Stunde/Tag, Historie wie weit zurück), wo sie liegen (RAM, Dateien, settings, DLS), in welchen Strukturen, mit welchen Sperren/Tasks, und welche "kleine Schnittstelle" für eine App ohne Systemrechte nötig wäre (Syscalls, Kopien, Grösse). Ausserdem: was Boulder (App) heute an Werten berechnet, die die Uhr nicht hat (Urteile gegen 14/30 Tage, HRV-Band, Blutdruck, Gewicht, Wasser/Koffein/Präparate aus Drinktervall/SupCycle), und ob/wie sie zur Uhr kämen (AppMessage, Timeline, Blob-DB). Schreibe ${ORT}/schritt3-bestand-daten.md und gib Pfad + Kurzfassung zurück.`, { label: 'bestand-daten', phase: 'Bestand', schema: BESTAND }),
])

phase('Spezifikation')
const spez = await agent(`${GRUND}

Bestand: ${pebble ? pebble.datei : '(fehlt)'} und ${daten ? daten.datei : '(fehlt)'} vollständig lesen.

Schreibe ${ORT}/schritt3-spezifikation.md im Aufbau von Schritt 2, schlank: Ziel und Grenzen (was gehört in Schritt 3, was in Schritt 4 "App-Teile", z. B. Routinen-App); Bildschirme der Uhr-App als Liste mit Inhalt (keine Textwände; Vorschlag, der sich an Boulders Health-Tab anlehnt, aber auf 200×228 passt), Navigation und Tasten; Datenschnittstelle (Syscalls, Strukturen, Sperren, Speicher); Ersetzen von Pebbles Health-App nur auf emery (alle Einstiege: Startmenü, Quick Launch, Zifferblatt-Shortcuts, Insights); Einstellungen auf der Uhr (Health: Messstufe usw.; Abgrenzung zu Sport/Kieselsport); was vom Telefon kommt (falls nötig) und was nicht; Pebble-Eingriffe vollständig mit Zeilengrenzen; Arbeitspakete mit Abhängigkeiten, Tests, QEMU-Prüfweg und Bildbelegen; Abnahmekriterien (Entwurf, prüfbar); offene Fragen F-Liste mit je einer Empfehlung (nur echte Entscheidungen des Nutzers); Vorgaben zum Bestätigen (V-Liste). Ziel nach Schritt 3: erste Vorabversion für die Uhr (Gerätetest von Schritt 1–3) – nenne, was dafür noch zusätzlich nötig ist (Upstream v4.40.0 nachziehen, Wechsel b26 → V2, Boulder-Gegenstück). Gib eine Kurzfassung (max. 15 Zeilen) zurück: Bildschirm-Vorschlag, Zahl der Pakete, wichtigste offene Fragen.`, { label: 'spezifikation', phase: 'Spezifikation' })

phase('Gegenprüfung')
const pruefung = await agent(`${GRUND}

Prüfe ${ORT}/schritt3-spezifikation.md adversarial gegen den Code: Stimmt jede Tatsachenbehauptung (APIs, Rechte einer System-App ohne Systemrechte, Speicher, Display, Einstiege)? Fehlt ein Einstieg zu Pebbles Health-App (Shortcut, Insight, Timeline, Glance, Quick Launch, Watchface-Aktion)? Ist die Datenschnittstelle sicher (Sperren, Lebensdauer, Grösse)? Sind Kriterien prüfbar? Korrigiere Fehler DIREKT in der Spezifikation und hänge unten einen Abschnitt "Gegenprüfung" mit den Änderungen an. Gib die Liste der Korrekturen zurück.`, { label: 'gegenpruefung', phase: 'Gegenprüfung', effort: 'high' })
return { pebble, daten, spez, pruefung }
