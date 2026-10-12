export const meta = {
  name: 'v2-wecker-und-schritt2',
  description: 'V2: Wecker-Sicherung fertigstellen, dann Schritt 2 (Kieselsport auf dem Workout-Dienst) bauen, prüfen und abnehmen',
  phases: [
    { title: 'Wecker', detail: 'Zwischenstand fertigbauen, gegenprüfen, nachbessern' },
    { title: 'Schritt 2', detail: 'AP0 bis AP7 nacheinander (AP5+AP6 zusammen)' },
    { title: 'Prüfung', detail: 'eine Gegenprüfung über Schritt 2' },
    { title: 'Abnahme', detail: 'Nachbesserung und AP8 Integration mit Bericht' },
  ],
}

const ORT = '/home/dysseus/Documents/Claude/boulder-v2'
const S = '/tmp/claude-1000/-home-dysseus-Documents-Claude/a0d87cde-04f6-43b4-bc17-a2150aed9501/scratchpad'
const TRAILER = 'Commit-Nachrichten deutsch mit den Schlusszeilen:\nCo-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>\nClaude-Session: https://claude.ai/code/session_012RYr1ZRXBzb7Y9hLeMoBPc'
const GRUND = `Boulder V2, Firmware ~/Projects/pebbleos, Zweig v2 (origin/v2). Massgebend: ${ORT}/entscheidungen.md, ${ORT}/schritt2-spezifikation.md (Abschnitt 10 "Entscheide des Nutzers" geht Abschnitt 7 vor), Bestand ${ORT}/schritt2-bestand-kieselsport.md und ${ORT}/schritt2-bestand-workout.md, Vorbild und Regeln aus Schritt 1 (${ORT}/schritt1-spezifikation.md, ${ORT}/schritt1-abnahme.md).
Regeln: Pebble-Code nur an den entschiedenen Stellen mit "Boulder:"-Kommentar; Bezeichner deutsch, Kommentare zum WARUM; Tests bauen Produktionscode nicht nach; jede Kernaussage per Mutation belegen; Host-Test-Baseline 13 bekannte Upstream-Fehler, keiner mehr; BLE/Benachrichtigungen unberührt. Umgebung: . ~/pebbleos-sdk-0.1.8/env.sh && . ~/Projects/pebbleos/.venv/bin/activate; pbl aus dem Worktree (PYTHONPATH=<wt>/tools/libs/pbl-cli). Eigener Worktree je Paket unter ${S}/v2s2/ von origin/v2 (Submodule per --reference wie in Schritt 1). /tmp ist flüchtig: nach jedem Paket committen, auf den neuesten origin/v2 rebasen und pushen. main nicht anfassen. ${TRAILER}`

const ERG = { type: 'object', properties: {
  commits: { type: 'array', items: { type: 'string' } }, tests: { type: 'string' },
  abweichungen: { type: 'array', items: { type: 'string' } }, offen: { type: 'array', items: { type: 'string' } } },
  required: ['commits', 'tests', 'abweichungen', 'offen'] }
const BEFUNDE = { type: 'object', properties: { befunde: { type: 'array', items: { type: 'object', properties: {
  datei: { type: 'string' }, schwere: { type: 'string', enum: ['hoch', 'mittel', 'niedrig'] }, beschreibung: { type: 'string' }, beleg: { type: 'string' } },
  required: ['datei', 'schwere', 'beschreibung', 'beleg'] } } }, required: ['befunde'] }

phase('Wecker')
const wecker = await agent(`${GRUND}

WECKER (Entscheid 07.10.: diese Sicherheit will der Nutzer). Ein unterbrochener, ungeprüfter Zwischenstand liegt auf origin/v2-wecker-zwischenstand (af95791af; Worktree ${S}/v2-wecker existiert noch). Prüfe ihn kritisch, übernimm Brauchbares, stelle fertig:
- Neustart vor dem Fenster: unverändert. Neustart im Fenster: strenge Regel läuft weiter (nie im Tiefschlaf, spätestens zur Weckzeit). Neustart nach der Weckzeit, noch nicht geklingelt: sofort klingeln, nur innerhalb 30 min Nachholfenster (Konstante). Nie zweimal am selben Tag (auch nicht nach Klingeln/Wegdrücken vor dem Neustart); Schlummern wie Pebble; Einmal-/Wiederholungswecker, Zeitsprung, ausgeschaltete Wecker.
- Pebble-Code nur in alarm.c (Weckregel) mit "Boulder:". Host-Tests mit echtem alarm.c für alle Fälle, Mutationen, Kriterium-4-Tests aus Schritt 1 grün, Baseline unverändert.
- Als ein sauberer Commit auf origin/v2 (rebased), den Zwischenzweig danach löschen (git push origin --delete v2-wecker-zwischenstand). Nachtrag in ${ORT}/schritt1-abnahme.md (Kriterium 4, Lücke geschlossen).`, { label: 'wecker', phase: 'Wecker', schema: ERG })

const weckerPruefung = await agent(`${GRUND}

Gegenprüfung WECKER (nichts ändern): Commits ${wecker ? wecker.commits.join(', ') : '(den jüngsten Wecker-Commit auf origin/v2 nehmen)'}. Kann der Wecker doppelt, im Tiefschlaf vor der Weckzeit oder gar nicht klingeln (Grenzen, Schlummern, Wiederholung am Folgetag, Zeitsprung, kaputte/volle Persistenz)? Pebble-Code nur an der entschiedenen Stelle? Belege jeden Befund.`, { label: 'wecker-pruefung', phase: 'Wecker', schema: BEFUNDE, effort: 'high' })
if (weckerPruefung && weckerPruefung.befunde.length) {
  await agent(`${GRUND}

Behebe diese Befunde zum Wecker (jeden erst prüfen, falsche begründet ablehnen), Tests + Mutation, commit + push auf origin/v2:
${JSON.stringify(weckerPruefung.befunde, null, 2)}`, { label: 'wecker-nachbesserung', phase: 'Wecker', schema: ERG })
}

phase('Schritt 2')
const PAKETE = [
  ['AP0', 'AP0 Gerüst laut Spezifikation Abschnitt 5.'],
  ['AP1', 'AP1 Pulskalorien und Verwerfen (F3 = A: Merker in P1, verworfen speichert nichts).'],
  ['AP2', 'AP2 Dienst (F2 = A: Kieselsport hält das 1-Hz-Abo wie b25).'],
  ['AP3', 'AP3 App, Teile a und b.'],
  ['AP4', 'AP4 Variante. ACHTUNG V1 geändert: 0 Schritte (Summe, Minute, Rate) für ALLE Arten ohne Schritte (Velo, MTB, Kraft, Yoga, Schwimmen); Schritte zählen nur bei Laufen, Wandern, Gehen.'],
  ['AP5+6', 'AP5 Sichtbarkeit und DLS 84 (F4 = A, nur mit CONFIG_BOULDER_KIESELSPORT) und AP6 Wechsel b25 → V2 (V9 bestätigt: alte Store-App nicht ausblenden, nur prüfen und in den Bericht).'],
  ['AP7', 'AP7 QEMU und Ressourcen.'],
]
const ergebnisse = []
for (const [key, text] of PAKETE) {
  const vorher = ergebnisse.map(e => `${e.key}: ${e.r ? e.r.commits.join(', ') + ' | offen: ' + e.r.offen.join(' / ') : 'ausgefallen'}`).join('\n')
  const r = await agent(`${GRUND}

Bisherige Pakete in Schritt 2:
${vorher || '(keine)'}

${text} Umsetzen mit den Tests und Prüfwegen der Spezifikation (Abschnitt 5 und 6.1), Mutationen je Kernaussage, betroffene Tests und die Bauten obelix + qemu_emery grün, Baseline unverändert. Weiche nur ab, wo der Code es erzwingt, mit Grund.`, { label: `s2-${key}`, phase: 'Schritt 2', schema: ERG })
  ergebnisse.push({ key, r })
  log(`${key}: ${r ? r.commits.length + ' Commits' : 'ausgefallen'}`)
}

phase('Prüfung')
const pruefung = await agent(`${GRUND}

Gegenprüfung Schritt 2 (nichts ändern): Prüfe die Commits von Schritt 2 auf origin/v2 gegen Spezifikation, Abschnitt 10 und Kriterien K1–K8. Schwerpunkte: Sperrfolgen (Workout → Activity → Kraepelin, HRM-Rückrufe), Verwerfen speichert wirklich nichts, keine Doppelsitzungen (Training vs. erkannte Fahrt, Trainingsstart während Fahrt), 0 Schritte für alle Nicht-Schritt-Arten (V1 neu), Puls-Abo endet sicher, DLS 84 Fassung 7 nur mit CONFIG, Pebble-Eingriffe nur P1–P4 (+ alarm.c, settings/health.c), Persistenz/Wechsel b25, Ressourcen. Belege jeden Befund (Test, Codepfad, QEMU).`, { label: 's2-pruefung', phase: 'Prüfung', schema: BEFUNDE, effort: 'high' })

phase('Abnahme')
let nach = null
if (pruefung && pruefung.befunde.length) {
  nach = await agent(`${GRUND}

Behebe diese Befunde der Gegenprüfung Schritt 2 (jeden zuerst prüfen; falsche begründet ablehnen), Tests + Mutation, commit + push:
${JSON.stringify(pruefung.befunde, null, 2)}`, { label: 's2-nachbesserung', phase: 'Abnahme', schema: ERG })
}
const abnahme = await agent(`${GRUND}

AP8 Integration: alle Boards bauen, voller Host-Testlauf (Baseline), Kriterien von Schritt 1 erneut, alle Kriterien K1–K8 von Schritt 2 mit Belegstand je Kriterium (erfüllt / teilweise / offen und was dafür fehlt, z. B. echte Referenztrainings für K5). Bericht ${ORT}/schritt2-abnahme.md. Commit + push. Antworte mit einer Kurzfassung für den Nutzer (deutsch, ohne Fachjargon, max. 15 Zeilen) und den Entscheidungen, die er treffen muss.`, { label: 's2-abnahme', phase: 'Abnahme' })

return { wecker, weckerPruefung, ergebnisse, pruefung, nach, abnahme }
