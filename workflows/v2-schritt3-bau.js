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

phase('Bau')
const PAKETE = ['AP0', 'AP1', 'AP5', 'AP7', 'AP2', 'AP3', 'AP4', 'AP6', 'AP8']
const ergebnisse = []
for (const ap of PAKETE) {
  const vorher = ergebnisse.map(e => `${e.ap}: ${e.r ? e.r.commits.join(', ') + ' | offen: ' + e.r.offen.join(' / ') : 'ausgefallen'}`).join('\n')
  const r = await agent(`${GRUND}

Bisherige Pakete in Schritt 3:
${vorher || '(keine)'}

Setze ${ap} laut Spezifikation Abschnitt 8 um, mit den dort genannten Tests und Prüfwegen (8.1, 9.1), Mutationen je Kernaussage, betroffene Tests und obelix + qemu_emery grün, Baseline unverändert. Bildbelege (QEMU-Schirme) selbst ansehen und nach ${ORT}/belege-schritt3/ legen. Weiche nur ab, wo der Code es erzwingt, mit Grund.`, { label: `s3-${ap}`, phase: 'Bau', schema: ERG })
  ergebnisse.push({ ap, r })
  log(`${ap}: ${r ? r.commits.length + ' Commits' : 'ausgefallen'}`)
}

phase('Prüfung')
const pruefung = await agent(`${GRUND}

Gegenprüfung Schritt 3 (nichts ändern): Prüfe die Commits von Schritt 3 auf origin/v2 gegen Spezifikation und Entscheide. Schwerpunkte: Grundsatz (jede angezeigte Zahl aus Uhrdaten, kein Koffein/keine Getränke, nichts vom Telefon ausser den Zielen); App wirklich ohne Systemrechte (keine beschreibbaren static, nur erlaubte Syscalls, Syscall-Stapel); Syscalls sicher (Grenzen, Kopien, Sperren, Aufträge an KernelBG, Obergrenze je Sekunde, nichts wartet ohne Frist); Ladezeit-Ziele wirklich gemessen; Ziele-Filter (Einfügen/Löschen/Leeren, Länge 8 B, Vorgabe); Einstiege (Taste Oben, Startmenü, Quick Launch-Varianten, Pins, Start vom Telefon); andere Bretter behalten Pebbles App; Speicher (Kernel-RAM-Zuwachs, Schreiblast). Belege jeden Befund.`, { label: 's3-pruefung', phase: 'Prüfung', schema: BEFUNDE, effort: 'high' })

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
