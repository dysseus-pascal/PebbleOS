export const meta = {
  name: 'boulder-052-korrekturen',
  description: 'Boulder 0.52.0: Body in Heart & Body, ein Trends-Weg, HRV/Blutdruck-Trendband, Pulsbezug im Tagesband, HRV absichern und reparieren',
  phases: [
    { title: 'Bau', detail: 'P1 bis P3 nacheinander, je Commit und Push' },
    { title: 'Prüfung', detail: 'eine adversariale Gegenprüfung, Schwerpunkt Daten' },
    { title: 'Nachbesserung', detail: 'Befunde beheben' },
    { title: 'Abschluss', detail: 'Version, Notiz, Gesamttest, Release-Build, Bilder' },
  ],
}

const SPEZ = '/home/dysseus/Documents/Claude/boulder-v1/korrekturen-0.52-spezifikation.md'
const WT = '/home/dysseus/Projects/boulder-052'
const TRAILER = 'Commit-Nachrichten deutsch und mit diesen zwei Schlusszeilen:\nCo-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>\nClaude-Session: https://claude.ai/code/session_012RYr1ZRXBzb7Y9hLeMoBPc'
const GEMEINSAM = `Lies zuerst die Spezifikation ${SPEZ} vollständig, dazu die dort genannten Grundregeln (Spezifikation 0.51) und die Inputs-Datei. Arbeitskopie: ${WT} (Zweig korrekturen-0.52). ${TRAILER}\nNach deinem Paket: committen und \`git push -u origin korrekturen-0.52\`. Kein Push auf main, kein Tag, kein Release. Parallel läuft auf dem Rechner ein Firmware-Workflow in ~/Projects/pebbleos – nicht anfassen.`

const PAKET = {
  type: 'object',
  properties: {
    commits: { type: 'array', items: { type: 'string' } },
    tests: { type: 'string' },
    pngs: { type: 'array', items: { type: 'string' } },
    abweichungen: { type: 'array', items: { type: 'string' } },
    offen: { type: 'array', items: { type: 'string' } },
  },
  required: ['commits', 'tests', 'pngs', 'abweichungen', 'offen'],
}

phase('Bau')
const PAKETE = [
  { key: 'P1', text: 'Paket P1 – Heart & Body und Health-Liste (Punkte 1 bis 4 von P1).' },
  { key: 'P2', text: 'Paket P2 – Tagesband: Bezugslinien hinter dem Puls.' },
  { key: 'P3', text: 'Paket P3 – HRV-Daten absichern und einmalig reparieren. Das ist der riskante Teil: Health Connect nur LESEN, nie schreiben oder löschen; eigene Nächte nur bei hrv 0/null ändern.' },
]
const ergebnisse = []
for (const p of PAKETE) {
  const vorher = ergebnisse.map(e => `${e.key}: ${e.r ? e.r.commits.join(', ') : 'ausgefallen'}`).join('\n')
  const r = await agent(`${GEMEINSAM}

Bisherige Pakete:
${vorher || '(keine)'}

${p.text}

Vorgehen: umsetzen, Tests schreiben (Kernaussagen mit Mutation belegen), betroffene Modul-Tests laufen lassen, Bildbelege zeichnen und nach ~/Documents/Claude/boulder-v1/belege-0.52/${p.key.toLowerCase()}/ kopieren, Pfade melden. Abweichungen nur, wo der Code es erzwingt, mit Grund.`, { label: `paket-${p.key}`, phase: 'Bau', schema: PAKET })
  ergebnisse.push({ key: p.key, r })
  log(`${p.key}: ${r ? r.commits.length + ' Commits, ' + r.pngs.length + ' Bilder' : 'ausgefallen'}`)
}

phase('Prüfung')
const BEFUNDE = {
  type: 'object',
  properties: { befunde: { type: 'array', items: { type: 'object', properties: {
    datei: { type: 'string' }, zeile: { type: 'integer' }, schwere: { type: 'string', enum: ['hoch', 'mittel', 'niedrig'] },
    beschreibung: { type: 'string' }, beleg: { type: 'string' } }, required: ['datei', 'schwere', 'beschreibung', 'beleg'] } } },
  required: ['befunde'],
}
const pruefung = await agent(`${GEMEINSAM}

Du bist die Gegenprüfung (nichts ändern, nichts committen). Prüfe \`git -C ${WT} diff main...korrekturen-0.52\` gegen die Spezifikation. Schwerpunkt P3 (Daten):
- Kann die Merge-Regel je eine echte neue HRV verwerfen oder eine falsche behalten? Gilt sie auch für andere Speicherwege derselben Nacht (Umzug, Import, Teilsätze)?
- Reparatur: liest sie wirklich nur eigene Sätze (Herkunft + clientRecordId), nur 1..200, nur bei 0/null, nur einmal, schreibt/löscht sie nie in Health Connect, ist sie ausserhalb der Verbindungs- und Benachrichtigungswege, loggt sie, was passiert ohne Berechtigung, was bei Absturz mitten drin?
- jeTag-Wahl: verändert sie etwas ausser der HRV-Woche?
Dann P1/P2: Band ab 3, Urteil ab 7; Blutdruck-Strich; nur ein Trends-Weg; Body-Karte nur mit Daten und Link zur Körperseite; Bezugslinien, Skala, Zahlen am Rand; Grundregeln (Englisch, keine Textwände, Baukasten, catch mit Log, Tests ohne Nachbau).
Sieh dir die PNGs an (Read) und vergleiche mit dem Entwurf Erlebnis-Herz.dc.html. Bildpfade: ${ergebnisse.map(e => e.r ? e.r.pngs.join(', ') : '').join(', ')}
Melde nur belegbare Befunde.`, { label: 'pruefung', phase: 'Prüfung', schema: BEFUNDE, effort: 'high' })

phase('Nachbesserung')
let nachbesserung = null
const befunde = pruefung ? pruefung.befunde : []
if (befunde.length) {
  nachbesserung = await agent(`${GEMEINSAM}

Behebe diese Befunde der Gegenprüfung. Prüfe jeden zuerst selbst; ist einer falsch, begründe das statt zu ändern. Tests ergänzen, betroffene Tests laufen lassen, geänderte Bildschirme neu zeichnen.

${JSON.stringify(befunde, null, 2)}`, { label: 'nachbesserung', phase: 'Nachbesserung', schema: PAKET })
} else {
  log('Keine Befunde – Nachbesserung entfällt')
}

phase('Abschluss')
const abschluss = await agent(`${GEMEINSAM}

Abschluss für 0.52.0 auf dem Zweig (nicht auf main), wie in der Spezifikation unter "Abschluss":
1. BOULDER_VERSION 0.52.0, Release-Notiz .github/release/0.52.0.md (deutsch, erste Zeile Titel, jeder Satz belegt, ehrlich zu So/Mo ohne HRV und zur Prüftiefe).
2. ci/debug.keystore nach $ANDROID_USER_HOME/debug.keystore, dann Gesamttest mit --rerun; Ergebnis je Modul.
3. Release-APK mit apksigner prüfen (Zertifikat beginnt mit 212e0ca4), Pfad, Grösse, SHA-256, versionCode/versionName melden.
4. Alle Bildbelege neu zeichnen nach ~/Documents/Claude/boulder-v1/belege-0.52/abschluss/ und die Pfade melden.
5. Committen und pushen.`, { label: 'abschluss', phase: 'Abschluss', schema: PAKET })

return { ergebnisse, pruefung, nachbesserung, abschluss }
