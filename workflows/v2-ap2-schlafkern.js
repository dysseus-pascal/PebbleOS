export const meta = {
  name: 'v2-ap2-schlafkern',
  description: 'V2 Schritt 1, AP2 Schlafkern (schlank, ein Agent): b26-Schlafanalyse in die Kraepelin-Variante, Gleichheit gegen Referenzbytes',
  phases: [{ title: 'AP2', detail: 'ein Agent' }],
}
const S = '/tmp/claude-1000/-home-dysseus-Documents-Claude/a0d87cde-04f6-43b4-bc17-a2150aed9501/scratchpad'
const V2 = '/home/dysseus/Documents/Claude/boulder-v2'
phase('AP2')
const r = await agent(`Boulder V2, Schritt 1, Arbeitspaket AP2 "Schlafkern". SCHLANK arbeiten (Token-Verbrauch): keine Nebenrecherchen, keine langen Dokumente, Mutation nur fuer Kernaussagen, Bericht hoechstens 25 Zeilen.

LIES: ${V2}/entscheidungen.md (verbindlich) und in ${V2}/schritt1-spezifikation.md die Abschnitte Schlaf und AP2. AP0 ist fertig: Branch v2 (origin/v2, Worktree ${S}/v2s1/wt-basis; falls er fehlt: git -C ~/Projects/pebbleos worktree prune; worktree add ${S}/v2s1/wt-basis v2; Submodule mit --reference auf ~/Projects/pebbleos). Variante unter fw/services/activity/boulder/kraepelin, Referenzbytes b26 unter tests/fixtures/activity/boulder/referenz-b26/ (Generator tools/boulder/referenz_b26/erzeugen.py). Quelle der Regeln: ~/Projects/pebbleos main = b26 (src/fw/services/activity/boulder/schlafanalyse.c inkl. Waelz- und Aufsteh-Regel, nacht.c, Tests/Fixtures).
Umgebung: . ~/pebbleos-sdk-0.1.8/env.sh && . ~/Projects/pebbleos/.venv/bin/activate; pbl aus dem Worktree (PYTHONPATH=<wt>/tools/libs/pbl-cli). Nach jedem fertigen Teil committen (Trailer "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" und "Claude-Session: https://claude.ai/code/session_012RYr1ZRXBzb7Y9hLeMoBPc") und git push origin v2. main nicht anfassen.

AUFGABE AP2 (nach Spezifikation, Entscheidungen haben Vorrang):
- Den b26-Schlafkern (schlafanalyse.c als reine Rechnung) in v2 uebernehmen und nach dem Nachlauf-Prinzip der Spezifikation fuettern (fester Ring, kein Heap im Minutenpfad, keine Datei-Scans), ohne Pebble-Code ausserhalb der erlaubten Stellen zu beruehren (k1_waechter.py muss gruen bleiben).
- Pflicht-Nachweis: Fuer alle sechs Referenznaechte liefert der V2-Kern dieselben Phasen je Minute und denselben Nachtsatz 0x4001 wie b26 (0 abweichende Minuten). Abweichungen nur, wo die Spezifikation sie ausdruecklich begruendet - dann einzeln belegen.
- Mutation: eine Regel des Kerns verfaelschen -> Gleichheitstest rot.
- Host-Tests: keine neuen Fehler gegenueber der Baseline; obelix-Release-Bau ohne Warnung.
BERICHT (kurz): was gebaut, Commits, Gleichheit je Nacht, Rechenzeit/Speicher grob, Abweichungen, offene Punkte.`, { label: 'ap2', phase: 'AP2' })
return r
