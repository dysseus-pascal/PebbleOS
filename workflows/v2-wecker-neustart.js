export const meta = {
  name: 'v2-wecker-neustart',
  description: 'V2: Wecker klingelt auch nach Neustart im Weckfenster (alarm.c), mit Tests und Gegenprüfung',
  phases: [
    { title: 'Bau', detail: 'Fix in alarm.c/wecker.c mit Tests' },
    { title: 'Prüfung', detail: 'Gegenprüfung, bei Befund Nachbesserung' },
  ],
}

const ORT = '/home/dysseus/Documents/Claude/boulder-v2'
const S = '/tmp/claude-1000/-home-dysseus-Documents-Claude/a0d87cde-04f6-43b4-bc17-a2150aed9501/scratchpad'
const GRUND = `Boulder V2, Firmware ~/Projects/pebbleos, Zweig v2 (origin/v2). Lies ${ORT}/entscheidungen.md (Abschnitte Smart Alarm/F-Liste und "Nach Schritt 1"), ${ORT}/schritt1-abnahme.md (Kriterium 4) und ${ORT}/schritt1-spezifikation.md (Weckregel, E4, AP7). Die strenge Weckregel kam mit Commit 04ef7ad34 (alarm.c, wecker.c).
Entscheid des Nutzers (07.10.2026): Startet die Uhr später als 5 min nach Beginn des Weckfensters neu, soll der Wecker trotzdem klingeln – "diese Sicherheit möchte ich haben".
Regeln: Pebble-Code nur an der entschiedenen Stelle (alarm.c, Weckregel) mit Kommentar "Boulder:"; Boulder-Code in den Boulder-Dateien. Kommentare erklären das WARUM, Bezeichner deutsch. Tests bauen den Produktionscode nicht nach (echter alarm.c wie in T4.2). Jede Kernaussage per Mutation belegen. Baseline der Host-Tests: 13 bekannte Upstream-Fehler, keiner mehr. Umgebung: . ~/pebbleos-sdk-0.1.8/env.sh && . ~/Projects/pebbleos/.venv/bin/activate; pbl aus dem Worktree via PYTHONPATH=<wt>/tools/libs/pbl-cli. Eigener Worktree unter ${S}/v2-wecker von origin/v2 (Submodule per --reference wie in der Schritt-1-Grundlage). Commit-Nachricht deutsch mit den Schlusszeilen:
Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_012RYr1ZRXBzb7Y9hLeMoBPc
Danach auf origin/v2 pushen (vorher auf den neuesten origin/v2 rebasen). main nicht anfassen.`

const ERG = {
  type: 'object',
  properties: {
    commit: { type: 'string' }, verhalten: { type: 'string', description: 'Regel in Worten: was passiert bei Neustart vor, im und nach dem Fenster' },
    tests: { type: 'string' }, abweichungen: { type: 'array', items: { type: 'string' } }, offen: { type: 'array', items: { type: 'string' } },
  },
  required: ['commit', 'verhalten', 'tests', 'abweichungen', 'offen'],
}

phase('Bau')
const bau = await agent(`${GRUND}

Aufgabe: Finde heraus, warum der Wecker nach einem Neustart mehr als 5 min nach Fensterbeginn nicht klingelt (mit Datei:Zeile), und behebe es so:
- Neustart VOR dem Fenster: unverändert.
- Neustart IM Fenster (vor der Weckzeit): die strenge Regel läuft weiter – nie im Tiefschlaf wecken, spätestens zur Weckzeit klingeln.
- Neustart NACH der Weckzeit, wenn der Wecker vor dem Neustart noch nicht geklingelt hat: sofort nach dem Start klingeln, aber nur innerhalb eines Nachholfensters (Vorschlag 30 min nach der Weckzeit; als Konstante, im Bericht als offene Zahl nennen).
- Nie zweimal für denselben Wecker am selben Tag klingeln (auch nicht, wenn er vor dem Neustart schon geklingelt hat oder weggedrückt wurde); Schlummern bleibt wie bei Pebble.
- Einmal- und Wiederholungswecker, Zeitzonen-/Uhrzeitsprung und ausgeschaltete Wecker beachten.
Tests: Host-Tests mit dem echten alarm.c für alle Fälle oben (Neustart vor/im Fenster, nach Weckzeit innerhalb/ausserhalb Nachholfenster, schon geklungen, weggedrückt, Schlummern), Mutationen je Regel. Wenn möglich ein QEMU-Ablauf (Neustart im Fenster). Kriterium-4-Tests aus Schritt 1 bleiben grün. Bericht auch als Nachtrag in ${ORT}/schritt1-abnahme.md (Kriterium 4: Lücke geschlossen, mit Commit).`, { label: 'wecker-bau', phase: 'Bau', schema: ERG })

phase('Prüfung')
const BEFUNDE = { type: 'object', properties: { befunde: { type: 'array', items: { type: 'object', properties: {
  datei: { type: 'string' }, schwere: { type: 'string', enum: ['hoch', 'mittel', 'niedrig'] }, beschreibung: { type: 'string' }, beleg: { type: 'string' } },
  required: ['datei', 'schwere', 'beschreibung', 'beleg'] } } }, required: ['befunde'] }
const pruefung = await agent(`${GRUND}

Gegenprüfung (nichts ändern): Prüfe den Commit ${bau ? bau.commit : '(unbekannt – den jüngsten Wecker-Commit auf origin/v2 nehmen)'} gegen den Entscheid und die Regeln. Kann der Wecker jetzt doppelt klingeln, im Tiefschlaf vor der Weckzeit klingeln, gar nicht klingeln (Neustart genau an Grenzen, Schlummern, Wiederholungswecker am Folgetag, Zeitsprung), Persistenz-Fehler beim Neustart (Flash voll, Datei kaputt)? Ist Pebble-Code nur an der entschiedenen Stelle geändert? Belege jeden Befund (Test, Codepfad).
Behauptetes Verhalten: ${bau ? bau.verhalten : ''}`, { label: 'wecker-pruefung', phase: 'Prüfung', schema: BEFUNDE, effort: 'high' })

let nach = null
if (pruefung && pruefung.befunde.length) {
  nach = await agent(`${GRUND}

Behebe diese Befunde der Gegenprüfung (jeden zuerst selbst prüfen; falsche begründet ablehnen), mit Tests und Mutation, commit + push auf origin/v2:
${JSON.stringify(pruefung.befunde, null, 2)}`, { label: 'wecker-nachbesserung', phase: 'Prüfung', schema: ERG })
}
return { bau, pruefung, nach }
