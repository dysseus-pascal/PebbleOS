export const meta = {
  name: 'b26-aufsteh-regel',
  description: 'b26: Aufsteh-Regel (kein Verlaengern der Nacht durch ruhiges Sitzen nach dem Aufstehen) in schlafanalyse.c, Tests mit beiden Exporten der Nacht 05.10., Gegenpruefung mit Nachbesserungsschleife',
  phases: [
    { title: 'Einbauen', detail: 'Aufsteh-Regel im b26-Worktree' },
    { title: 'Gegenpruefen', detail: 'Skeptiker, ggf. Nachbesserung' },
  ],
}

const S = '/tmp/claude-1000/-home-dysseus-Documents-Claude/a0d87cde-04f6-43b4-bc17-a2150aed9501/scratchpad'
const V2 = '/home/dysseus/Documents/Claude/boulder-v2'
const WT = S + '/b26/wt'
const LAGE = `KONTEXT: Boulder-Firmware V1 (~/Projects/pebbleos). Branch b26-einschlafen (Worktree ${WT}) enthaelt schon: Fixture Nacht 05.10., nachgeschaerfte Einschlafregel "Waelzen" (Fenster 40 min, 9/16), Test Einschlafen/Aufwachen je Fitbit-Nacht +-10 min. Berichte: ${V2}/b26-auswertung.md, ${V2}/b26-einbau.md (lesen).
Nur die reine Schlafrechnung src/fw/services/activity/boulder/schlafanalyse.c (+ Header, Tests, Fixtures) aendern. nacht.c, hrv.c, Sitzungen, Datalogging, Persistenz, Laufzeit NICHT anfassen (b25 laeuft beim Nutzer stabil). Satzformat 0x4001/0x4002 unveraendert.
NEUER BEFUND DES NUTZERS (Nacht 5./6.10., gleiche Nacht zweimal exportiert):
- ${V2}/naechte/nacht-2026-10-05.csv (Export 06:18): aufwachen=600 (06:00, Wecker; vom Nutzer bestaetigt korrekt).
- ${V2}/naechte/nacht-2026-10-05-morgen.csv (spaeterer Export, Minuten bis 07:24): aufwachen=644, schlaf=485. Der Nutzer ist um 06:00 aufgestanden, hat 06:10-06:24 Kaffee gemacht (94 Schritte, viel Bewegung) und sass 06:26-06:43 ruhig mit dem Kaffee (Puls 54-59 = Ruhepuls). b25 wertete 06:38-06:43 als Leichtschlaf (auch 06:41 mit 15 Schritten) und verlaengerte die Nacht bis 06:44; 06:00-06:37 wurden "Wach in der Nacht". Ursache: Die Nacht bleibt nach dem Aufwachen offen (nacht.c: fertig nach 15 min wach, endgueltig nach 120 min), und erneuter "Schlaf" in diesem Fenster laesst sie wachsen (gedacht fuer den Toilettengang nachts).
AUFSTEH-REGEL (vom Nutzer freigegeben, Schwellen datenbasiert festlegen):
1. Aufgestanden: nach dem Aufwachen Gehen, etwa >= 3 Minuten mit Schritten innerhalb von 15 Minuten (genaue Schwelle an den Daten belegen).
2. Danach verlaengert erneuter Schlaf die Nacht nur, wenn er mindestens 30 Minuten am Stueck dauert. Kuerzere Ruhe (Kaffee, Zeitung, Sofa) zaehlt nicht.
3. Minuten mit Schritten sind nie Schlaf - ACHTUNG: pruefe an ALLEN Fixture-Naechten, ob es nachts im Bett Phantom-Schritte gibt (Arm-Bewegungen, Kratzen). Wenn ja, die Regel so fassen, dass sie nachts keine falschen Wachminuten erzeugt (z. B. erst nach "aufgestanden" oder erst ab einer Mindestzahl Schritte) und das begruenden.
4. Nachts (Toilettengang) bleibt alles wie bisher: danach schlaeft man lange weiter, die Nacht waechst wie gewollt. Muss per Test belegt sein (echte Nacht mit eingefuegtem Toilettengang: 3-6 min mit Schritten um 03:00, danach Schlaf -> Nacht reicht bis zum echten Aufwachen, keine Folgenacht, kein frueheres Ende).
UMGEBUNG: . ~/pebbleos-sdk-0.1.8/env.sh && . ~/Projects/pebbleos/.venv/bin/activate; Submodule fuer Builds per Symlink aus ~/Projects/pebbleos einhaengen und am Ende aushaengen. Host-Tests: cmake -G Ninja -S tests -B <dir>; cmake --build <dir> -- -k 0; ctest --timeout 300 (bekannte 11 Upstream-Fehler ausgenommen). Code-Regeln: deutsche Bezeichner, Kommentare erklaeren das WARUM, Tests rufen die echte schlafanalyse auf (kein Nachbau), Mutation je Regelteil, kein catch ohne Log. Nach jedem fertigen Teilschritt committen (Trailer:
Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_012RYr1ZRXBzb7Y9hLeMoBPc). Nicht pushen, nicht mergen. Berichte auch nach ${V2}/b26-*.md. Deutsch.`

phase('Einbauen')
let bericht = await agent(`${LAGE}

AUFGABE "Aufsteh-Regel einbauen" im Worktree ${WT}:
1. Fixture nacht-2026-10-05-morgen.csv aufnehmen (Format wie die anderen). Test: In BEIDEN Exporten liefert die Analyse Aufwachen 06:00 (Minute 600); Schlafminuten und Phasen bis 06:00 in beiden gleich.
2. Regel 1-4 in schlafanalyse.c umsetzen. Schwellen datenbasiert: Werte aller Fixture-Naechte (26.09. Snapshot, 29.09., 30.09., 03.10., 05.10. beide) auswerten; Phantom-Schritte im Bett zaehlen und berichten.
3. Tests: Morgen-Fall (beide Exporte), Toilettengang-Fall (Regel 4), Schritte-Regel, alle bisherigen Tests (Fitbit +-10 min, Untergrenzen, Snapshot 26.09., Live-Zustand) gruen. Mutation je Regelteil (zuruecknehmen -> zugehoeriger Test rot).
4. Alle Host-Tests und obelix-Release-Bau (pbl build).
Bericht: Regel mit Schwellen und Begruendung, Zahlen je Nacht vorher/nachher (Einschlafen, Aufwachen, Schlaf, Uebereinstimmung), Phantom-Schritte, Tests, Mutationen, Commits. Auch nach ${V2}/b26-aufstehen.md.`, { label: 'aufstehen:einbauen', phase: 'Einbauen' })

phase('Gegenpruefen')
let urteil = null
for (let runde = 1; runde <= 2; runde++) {
  urteil = await agent(`${LAGE}

BERICHT DES EINBAUS (Runde ${runde}):\n${bericht}

Du bist Skeptiker. Pruefe den GESAMTEN Branch b26-einschlafen (git diff main..b26-einschlafen im Worktree ${WT}; nur lesen; eigene Proben unter ${S}/b26/gegen${runde}/):
(1) Wurde nur die reine Rechnung geaendert (keine Laufzeit, kein Satzformat)?
(2) Ueberanpassung: Einschlafregel und Aufsteh-Regel an zu wenigen Naechten eingestellt? Nachrechnen.
(3) Gegenfaelle nachstellen (mit der echten schlafanalyse): Toilettengang nachts mit Schritten; Nickerchen nach dem Aufstehen am Wochenende (z. B. 40 min wieder hinlegen um 07:30); Nachtschicht/spaetes Zubettgehen; Messstufe 1 bzw. Naechte ohne Puls; unruhige Nacht mit vielen Phantom-Schritten; Aufstehen ohne Schritte (Uhr am Ladekabel?).
(4) Pruefen die Tests das Behauptete (echte Analyse, kein kopierter Code), greifen die Mutationen?
(5) Sind die Berichtsaussagen belegt?
Urteil: "FREIGEBEN" oder "NACHBESSERN" mit konkreter Liste. Deutsch, knapp. Auch nach ${V2}/b26-gegenpruefung-${runde}.md.`,
    { label: `gegen:runde${runde}`, phase: 'Gegenpruefen' })
  if (!urteil || !/NACHBESSERN/.test(urteil) || runde === 2) break
  bericht = await agent(`${LAGE}

GEGENPRUEFUNG (Runde ${runde}):\n${urteil}

AUFGABE: Bessere im Worktree ${WT} alle berechtigten Punkte nach (am Code nachpruefen; unberechtigte mit Begruendung zurueckweisen), mit Tests und Mutation, alle Host-Tests und obelix-Bau. Bericht wie zuvor, auch nach ${V2}/b26-nachbesserung-${runde}.md.`,
    { label: `nachbessern:runde${runde}`, phase: 'Gegenpruefen' })
}
return { bericht, urteil }
