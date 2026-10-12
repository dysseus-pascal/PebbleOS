export const meta = {
  name: 'v2-schritt3-bau',
  description: 'V2 Schritt 3 bauen: eigene Health-App auf der Uhr (AP0–AP8), Gegenprüfung, Nachbesserung, AP9 Abnahme',
  phases: [
    { title: 'Bau', detail: 'AP0 bis AP8 nacheinander, je Commit und Push auf origin/v2' },
    { title: 'Prüfung', detail: 'eine adversariale Gegenprüfung' },
    { title: 'Abnahme', detail: 'Nachbesserung und AP9 Integration mit Bericht' },
  ],
}
const ORT = '/home/dysseus/Documents/Claude/boulder-v2'
const S = '/tmp/claude-1000/-home-dysseus-Documents-Claude/a0d87cde-04f6-43b4-bc17-a2150aed9501/scratchpad'
const GRUND = `Boulder V2, Firmware ~/Projects/pebbleos, Zweig v2 (origin/v2). Massgebend: ${ORT}/schritt3-spezifikation.md (Fassung 2 inkl. "Gegenprüfung 2"), ${ORT}/entscheidungen.md Abschnitt "Schritt 3 – Health-App auf der Uhr" mit F1–F10 (Grundsatz: Uhr zeigt nur selbst gesammelte Daten; kein Koffein, keine Getränke/Präparate in der Health-App; Karte "Heart"; kein Tagesband; schlanke HRV/Ruhepuls-Liste; Ziele über BlobDB Health boulder_ziele; Ladezeit-Ziele L1–L6). Bestand: ${ORT}/schritt3-bestand-pebble-health.md, ${ORT}/schritt3-bestand-daten.md. Vorbild für Regeln und Arbeitsweise: Schritt 1/2 (${ORT}/schritt2-abnahme.md).
Regeln: Pebble-Code nur P7 (und was die Spezifikation ausdrücklich erlaubt) mit "Boulder:" und Schalter; App ohne Systemrechte; Bezeichner deutsch, Kommentare zum WARUM, UI-Texte Englisch, keine Textwände; Tests bauen Produktionscode nicht nach; jede Kernaussage per Mutation; Host-Baseline 13 bekannte Upstream-Fehler; alle sechs Bretter ohne Warnung; BLE/Benachrichtigungen unberührt. Umgebung: . ~/pebbleos-sdk-0.1.8/env.sh && . ~/Projects/pebbleos/.venv/bin/activate; pbl aus dem Worktree. Eigener Worktree je Paket unter ${S}/v2s3/ von origin/v2 (Submodule per --reference). /tmp ist flüchtig: nach jedem Paket committen, auf den neuesten origin/v2 rebasen, pushen, Worktree entfernen (Platz in /tmp beachten). main nicht anfassen. Commit-Nachrichten deutsch mit den Schlusszeilen:
Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_012RYr1ZRXBzb7Y9hLeMoBPc`
const ERG = { type: 'object', properties: {
  commits: { type: 'array', items: { type: 'string' } }, tests: { type: 'string' },
  abweichungen: { type: 'array', items: { type: 'string' } }, offen: { type: 'array', items: { type: 'string' } } },
  required: ['commits', 'tests', 'abweichungen', 'offen'] }
const BEFUNDE = { type: 'object', properties: { befunde: { type: 'array', items: { type: 'object', properties: {
  datei: { type: 'string' }, schwere: { type: 'string', enum: ['hoch', 'mittel', 'niedrig'] }, beschreibung: { type: 'string' }, beleg: { type: 'string' } },
  required: ['datei', 'schwere', 'beschreibung', 'beleg'] } } }, required: ['befunde'] }

// Nur fuer die beim ersten Lauf ausgefallenen Pakete: ein leerer Zusatz laesst die Prompts von AP0-AP2 unveraendert (Cache)
const WIP = (ap) => ['AP3', 'AP4', 'AP6', 'AP8'].includes(ap) ? `

Hinweis: Der erste Lauf dieses Pakets brach am Nutzungslimit ab. Prüfe zuerst, ob ein lokaler Zweig v2s3-${ap.toLowerCase()} oder ein Worktree ${S}/v2s3/wt-${ap.toLowerCase()} mit WIP-Commits oder uncommitteten Änderungen existiert (bekannt: v2s3-ap3 mit zwei WIP-Commits auf 5854e5e01 in ${S}/v2s3/wt-ap3, dazu Bauordner). Wenn ja: lesen, am Stand prüfen und weiterführen statt neu beginnen; am Ende zu einem sauberen Commit zusammenfassen. Belege früherer Versuche in ${ORT}/belege-schritt3/ nicht ungeprüft übernehmen.` : ''

phase('Bau')
const PAKETE = ['AP0', 'AP1', 'AP5', 'AP7', 'AP2', 'AP3', 'AP4', 'AP6', 'AP8']
const ergebnisse = []
for (const ap of PAKETE) {
  const vorher = ergebnisse.map(e => `${e.ap}: ${e.r ? e.r.commits.join(', ') + ' | offen: ' + e.r.offen.join(' / ') : 'ausgefallen'}`).join('\n')
  const r = await agent(`${GRUND}

Bisherige Pakete in Schritt 3:
${vorher || '(keine)'}

Setze ${ap} laut Spezifikation Abschnitt 8 um, mit den dort genannten Tests und Prüfwegen (8.1, 9.1), Mutationen je Kernaussage, betroffene Tests und obelix + qemu_emery grün, Baseline unverändert. Bildbelege (QEMU-Schirme) selbst ansehen und nach ${ORT}/belege-schritt3/ legen. Weiche nur ab, wo der Code es erzwingt, mit Grund.${WIP(ap)}`, { label: `s3-${ap}`, phase: 'Bau', schema: ERG })
  ergebnisse.push({ ap, r })
  log(`${ap}: ${r ? r.commits.length + ' Commits' : 'ausgefallen'}`)
}

phase('Prüfung')
const LINSEN = [
  { key: 'grundsatz', p: 'Grundsatz und Inhalt: Stammt jede angezeigte Zahl auf jeder Karte und jedem Detail aus Uhrdaten? Kein Koffein, keine Getränke, keine Präparate, nichts vom Telefon ausser den Zielen. Karte heisst "Heart", kein Tagesband, schlanke HRV/Ruhepuls-Liste (F2). UI-Texte Englisch, keine Textwände. Karten und Details gegen Spezifikation und Boulder (gleiche Urteile, Formate, Farben mit Kontrast). QEMU-Bilder in belege-schritt3 selbst ansehen.' },
  { key: 'rechte', p: 'App ohne Systemrechte: keine beschreibbaren static/globals, nur erlaubte Syscalls und Pebble-Bausteine (Liste 3.6/3.7.5), Syscall-Stapel T2.5 mit allen Karten und Details, MenuLayer mit get_cell_height und eigenem Zeichnen (System-ID-Falle aus AP1), pruefe_ohne_rechte.py auf allen Bauten für die Uhr, Fehlerprobe nur im Prüfbau.' },
  { key: 'syscalls', p: 'Syscalls und Nebenläufigkeit: Zeiger- und Längenprüfung, Kopien statt Verweise, Sperren (Blattsperren, kein Pebble-Aufruf unter einer Sperre der Schnittstelle, keine Datei unter Sperre), Aufträge an KernelBG mit Merker, Obergrenze je Sekunde, nichts wartet ohne Frist, HRM-Abo der App (Auf- und Abbau, Kartenwechsel, App-Ende, Absturz), Wettläufe beim Boot und beim Umzug.' },
  { key: 'ladezeit', p: 'Ladezeit-Ziele L1 bis L6: wirklich gemessen (QEMU-Zeitstempel, Methode, Wiederholungen), nicht nur behauptet. Start ≤ 50 ms, Kartenwechsel ≤ 30 ms, kein Lesen beim Kartenwechsel, Cache je App-Start, Boulder-Syscalls ≤ 1 ms. Ist die Messung auf obelix übertragbar, und was fehlt bis zum Gerätetest?' },
  { key: 'einstiege', p: 'Einstiege und Ziele: Taste Oben, Startmenü, Quick Launch (alle Varianten), Pins (Activity/Night), Start vom Telefon, Richtung Oben/Unten (Pebble-Richtung statt b25, Kreis, Weg zum Zifferblatt). Andere Bretter behalten Pebbles App (T3.5). Ziele über BlobDB Health boulder_ziele: Filter für Einfügen/Löschen/Leeren, Länge 8 B, Fassung, Vorgabe 10 000/8 h, Neustart, Gültigkeit in Urteilen und Ziellinien.' },
  { key: 'speicher', p: 'Speicher, Daten und Tests: Kernel-RAM-Zuwachs (K7, über Abschnitte gemessen, Bezugspunkt), Ballast des Prüfbaus, Schreiblast auf dem Flash, Nächte-Liste (Grösse, Ort, Wechsel b26 → V2 ohne Datenverlust, Update mitten in der Nacht), K1-Wächter und P7. Bauen Tests Produktionscode nach? Sind die Mutationen echt (je Kernaussage rot)? Host-Baseline 13.' },
]
const BEFUND_PRUEFUNG = { type: 'object', properties: { echt: { type: 'boolean' }, schwere: { type: 'string', enum: ['hoch', 'mittel', 'niedrig'] }, begruendung: { type: 'string' } }, required: ['echt', 'schwere', 'begruendung'] }
const geprueft = await pipeline(LINSEN,
  (l) => agent(`${GRUND}

Gegenprüfung Schritt 3, Blickwinkel "${l.key}" (nichts ändern, nichts committen): Prüfe die Commits von Schritt 3 auf origin/v2 (ab 836b406e7) gegen Spezifikation und Entscheide. ${l.p}
Melde nur, was du mit Datei:Zeile, Testlauf oder Bild belegen kannst; Vermutungen ohne Beleg weglassen. Eigene Proben nur in einem Wegwerf-Worktree unter ${S}/v2s3/ und danach entfernen.`, { label: `s3-pruefung-${l.key}`, phase: 'Prüfung', schema: BEFUNDE, effort: 'high' }),
  (r, l) => (r && r.befunde.length) ? agent(`${GRUND}

Prüfe jeden dieser Befunde der Gegenprüfung (Blickwinkel "${l.key}") selbst am Code von origin/v2 nach und versuche ihn zu WIDERLEGEN (nichts ändern, nichts committen). Ein Befund ist nur echt, wenn du ihn selbst bestätigen kannst. Gib die Befunde zurück, die echt sind, mit der von dir festgestellten Schwere und deinem Beleg; widerlegte weglassen und unter "widerlegt" kurz begründen.
${JSON.stringify(r.befunde, null, 2)}`, { label: `s3-nachpruefung-${l.key}`, phase: 'Prüfung', schema: { type: 'object', properties: { befunde: BEFUNDE.properties.befunde, widerlegt: { type: 'array', items: { type: 'string' } } }, required: ['befunde', 'widerlegt'] }, effort: 'high' }) : { befunde: [], widerlegt: [] }
)
const pruefung = { befunde: geprueft.filter(Boolean).flatMap(g => g.befunde), widerlegt: geprueft.filter(Boolean).flatMap(g => g.widerlegt) }
log(`Gegenprüfung: ${pruefung.befunde.length} bestätigte Befunde, ${pruefung.widerlegt.length} widerlegt`)

phase('Abnahme')
let nach = null
if (pruefung && pruefung.befunde.length) {
  nach = await agent(`${GRUND}

Behebe diese Befunde der Gegenprüfung Schritt 3 (jeden zuerst prüfen; falsche begründet ablehnen), Tests + Mutation, commit + push:
${JSON.stringify(pruefung.befunde, null, 2)}`, { label: 's3-nachbesserung', phase: 'Abnahme', schema: ERG })
}
const abnahme = await agent(`${GRUND}

AP9 Integration: alle Bretter bauen, voller Host-Testlauf, Kriterien von Schritt 1 und 2 erneut, alle Kriterien von Schritt 3 mit Belegstand (erfüllt / teilweise / offen und was fehlt), Bildbelege der Karten und Details ansehen. Bericht ${ORT}/schritt3-abnahme.md, Belege in ${ORT}/belege-schritt3/. Commit + push. Liste am Ende, was für die erste Vorabversion auf der Uhr noch fehlt (Spezifikation Abschnitt 12: Upstream v4.40.0, Wechsel b26 → V2, Boulder-Gegenstück inkl. Ziele-Seite, Kieselstraße F6, Vorher-Video b26). Antworte mit einer Kurzfassung für den Nutzer (deutsch, ohne Fachjargon, max. 15 Zeilen) und den Entscheidungen, die er treffen muss.`, { label: 's3-abnahme', phase: 'Abnahme' })

return { ergebnisse, pruefung, nach, abnahme }
