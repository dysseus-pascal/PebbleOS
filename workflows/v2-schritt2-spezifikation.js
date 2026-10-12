export const meta = {
  name: 'v2-schritt2-spezifikation',
  description: 'V2 Schritt 2 auslegen: Kieselsport auf Pebbles Workout-Dienst – Bestand erheben, Spezifikation schreiben, gegenprüfen (nur lesen und schreiben in Dokumenten)',
  phases: [
    { title: 'Bestand', detail: 'Kieselsport und Workout-Dienst parallel erheben' },
    { title: 'Spezifikation', detail: 'schritt2-spezifikation.md schreiben' },
    { title: 'Gegenprüfung', detail: 'Spezifikation gegen den Code prüfen' },
  ],
}

const ORT = '/home/dysseus/Documents/Claude/boulder-v2'
const GRUND = `Kontext: Boulder V2 (Pebble-Firmware-Fork ~/Projects/pebbleos, Zweig v2 = origin/v2, Stand a01549098; App Boulder ~/Projects/Boulder; Uhr-App Kieselsport ~/Projects/Kieselsport). Schritt 1 (eigenes Kraepelin) ist fertig: ${ORT}/schritt1-abnahme.md, Spezifikation ${ORT}/schritt1-spezifikation.md (Aufbau als Vorbild). Entscheidungen: ${ORT}/entscheidungen.md (Abschnitt "Kieselsport (Weg 2)", F5, F7, Kalorien). Ideenliste: ${ORT}/ideen.md.
NICHTS in den Repos ändern, nichts committen. Deutsch, knapp, mit Datei:Zeile-Belegen.`

const KRITERIEN = `Entwurf der Abnahmekriterien Schritt 2 (vom Hauptagenten vorgeschlagen, noch nicht vom Nutzer bestätigt):
1. Pebble-Code nur an zwei Stellen: Pulskalorien im Workout-Dienst, Pebbles Trainings-App ausgeblendet; alles andere in Boulder-Dateien.
2. Pebble übernimmt Start, Pause, Stopp, 1-Hz-Puls, Erkennung am Arm, Speichern der Sitzung; jedes Training genau einmal in Pebbles Sitzungsliste, kein zweites Puls-Abo.
3. Gleiche Funktionen wie heute: Sportarten, Pausen mit Auto-Ende (30 min, Zonen-Hysterese 3 bpm), Zonen, Runden und Bahnen, Wiederholungen, GPS, Pulskurve; Boulder bekommt dieselben Trainingsdaten. Je Funktion Test oder QEMU-Ablauf.
4. Velo als "Offen": 0 Schritte; erkannte Fahrt aus Schritt 1 und gestartetes Velotraining ergeben keine Doppelsitzung.
5. Kalorien aus dem Puls nach der heutigen Formel, ±5 % zum heutigen Kieselsport bei Referenztrainings, festgelegter Ersatz ohne Puls.
6. Nur Kieselsport sichtbar (Startmenü, Schnellzugriffe); Einstellungen bleiben beim Wechsel erhalten.
7. 2-h-Training in QEMU ohne Leck/Watchdog; Start/Stopp-Folgen robust; 1-Hz-Puls nur im Training.
8. Kriterien von Schritt 1 bleiben grün, Upstream-Fehlerliste nicht länger.`

const BESTAND = {
  type: 'object',
  properties: { datei: { type: 'string' }, zusammenfassung: { type: 'string' } },
  required: ['datei', 'zusammenfassung'],
}

phase('Bestand')
const [kiesel, dienst] = await parallel([
  () => agent(`${GRUND}

Erhebe den Bestand von KIESELSPORT (~/Projects/Kieselsport, aktueller Stand) und seiner Gegenseite in Boulder: Aufbau (App/Worker), jede Funktion (Sportarten, Start/Pause/Stopp, Auto-Ende, Zonen und Hysterese, Runden/Bahnen, Wiederholungen, GPS über das Telefon, Navigation/Abbiegen-Vibration, Pulskurve, Kalorienformel, Live-Puls), eigene Puls-/Sensor-Abos, Persistenz, Einstellungen (Speicherorte, Schlüssel), Nachrichten an Boulder (AppMessage-Schlüssel, Formate, Empfang in Boulder mit Datei:Zeile), bekannte offene Fehler. Schreibe das Ergebnis nach ${ORT}/schritt2-bestand-kieselsport.md und gib Pfad + Kurzfassung zurück.`, { label: 'bestand-kieselsport', phase: 'Bestand', schema: BESTAND }),
  () => agent(`${GRUND}

Erhebe den Bestand von PEBBLES WORKOUT-DIENST auf origin/v2 (~/Projects/pebbleos; Zweig v2 lesen, z. B. per git show/worktree in /tmp/claude-1000/-home-dysseus-Documents-Claude/a0d87cde-04f6-43b4-bc17-a2150aed9501/scratchpad/s2-bestand): workout_service API (öffentlich für Apps? nur System?), Zustände Start/Pause/Stopp, 1-Hz-Puls und HRM-Abo, Erkennung am Arm, Sitzungsspeicherung (activity sessions, DLS), Kalorienrechnung (wo, welche Formel), Pebbles Workout-App (wie startet sie, wie lässt sie sich ausblenden, Schnellzugriffe), Schnittstellen zur Kraepelin-Variante aus Schritt 1 (Velo-Sitzung "Open" nach F5, kontext.c/nachfrage.c, Schritte während Training), was eine Drittanbieter-App (Kieselsport als .pbw) davon nutzen könnte bzw. was in die Firmware müsste. Schreibe nach ${ORT}/schritt2-bestand-workout.md und gib Pfad + Kurzfassung zurück.`, { label: 'bestand-workout', phase: 'Bestand', schema: BESTAND }),
])

phase('Spezifikation')
const spez = await agent(`${GRUND}

Bestand: ${kiesel ? kiesel.datei : '(fehlt)'} und ${dienst ? dienst.datei : '(fehlt)'} – beide vollständig lesen.
${KRITERIEN}

Schreibe ${ORT}/schritt2-spezifikation.md im Aufbau der Schritt-1-Spezifikation, aber schlank: Ziel und Grenzen; Architektur "Kieselsport auf dem Workout-Dienst" (was bleibt Uhr-App, was wird Firmware-Teil, wie die Komposition technisch geht – inkl. der Frage, ob Kieselsport eine .pbw bleibt oder in die Firmware wandert, mit Empfehlung); Datenflüsse zu Boulder (Kompatibilität mit Boulder 0.52); Arbeitspakete mit Abhängigkeiten, Tests und Prüfweg (Host-Tests/QEMU); Abnahmekriterien (den Entwurf übernehmen und wo der Bestand es verlangt präzisieren, Änderungen markieren); offene Fragen F-Liste mit Empfehlung je Frage (nur echte Entscheidungen des Nutzers). Gib eine Kurzfassung (max. 15 Zeilen) zurück: Architektur-Empfehlung, Zahl der Pakete, die wichtigsten offenen Fragen.`, { label: 'spezifikation', phase: 'Spezifikation' })

phase('Gegenprüfung')
const pruefung = await agent(`${GRUND}

Prüfe ${ORT}/schritt2-spezifikation.md adversarial gegen den Code (Kieselsport, Boulder, pebbleos v2): Stimmt jede Tatsachenbehauptung? Fehlt eine Funktion von Kieselsport? Ist die Architektur machbar (API-Grenzen von Apps vs. Firmware, Speicher, Sperren, Persistenz)? Sind Kriterien prüfbar? Korrigiere Fehler DIREKT in der Spezifikation (nur diese Datei) und hänge unten einen Abschnitt "Gegenprüfung" mit den Änderungen an. Gib die Liste der Korrekturen zurück.`, { label: 'gegenpruefung', phase: 'Gegenprüfung', effort: 'high' })

return { kiesel, dienst, spez, pruefung }
