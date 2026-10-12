export const meta = {
  name: 'v2-schritt2-nachtrag',
  description: 'V2 Schritt 2 Nachtrag: Schritte in der Pause wieder zählen, Startmenü wie b25 (P5/P6), Dokumente nachziehen; Gegenprüfung',
  phases: [
    { title: 'Bau', detail: 'Pause-Schritte (Boulder-Code), P5/P6 Startmenü' },
    { title: 'Prüfung', detail: 'Gegenprüfung, bei Befund Nachbesserung' },
  ],
}
const ORT = '/home/dysseus/Documents/Claude/boulder-v2'
const S = '/tmp/claude-1000/-home-dysseus-Documents-Claude/a0d87cde-04f6-43b4-bc17-a2150aed9501/scratchpad'
const GRUND = `Boulder V2, Firmware ~/Projects/pebbleos, Zweig v2 (origin/v2, Stand 8024da123). Massgebend: ${ORT}/entscheidungen.md, Abschnitt "Nach Schritt 2 (08.10.2026)" (Entscheide 1–5), ${ORT}/schritt2-spezifikation.md (P5/P6 in 2.3, AP3 Kachel), ${ORT}/schritt2-abnahme.md, Bestand ${ORT}/schritt2-bestand-kieselsport.md (b25-Fassung der Kachel und des Hochrückens auf main: src/fw/apps/system/launcher/default/app_glance_kieselsport.c, app_menu_data_source.c).
Regeln: Pebble-Code nur an den entschiedenen Stellen mit "Boulder:" und CONFIG_BOULDER_KIESELSPORT (andere Bretter unverändert); Bezeichner deutsch, Kommentare zum WARUM; Tests bauen Produktionscode nicht nach; jede Kernaussage per Mutation; Host-Baseline 13 bekannte Upstream-Fehler; alle sechs Bretter ohne Warnung; obelix-Heap melden. Umgebung: . ~/pebbleos-sdk-0.1.8/env.sh && . ~/Projects/pebbleos/.venv/bin/activate; pbl aus dem Worktree. Eigener Worktree unter ${S}/v2s2n/ von origin/v2 (Submodule per --reference). Nach dem Paket committen, auf den neuesten origin/v2 rebasen, pushen; Worktree danach entfernen. main nicht anfassen. Commit-Nachrichten deutsch mit den Schlusszeilen:
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
const bau = await agent(`${GRUND}

Setze um:
(2) Schritte in der Pause: Bei Sportarten ohne Schritte (Velo, MTB, Kraft, Yoga, Schwimmen) werden Schritte nur gestrichen, solange das Training LÄUFT; in der Pause und in den 30 s nach dem Speichern (V4) zählen sie wieder. Nur Boulder-Code (D17 / Variante). Tests: laufend 0, Pause zählt, Weiter wieder 0, nach Speichern/Verwerfen zählt; Laufen/Wandern/Gehen unverändert.
(3) Startmenü wie b25 (F6 jetzt A): Kieselsport rückt im Training im Startmenü nach oben (P5 app_menu_data_source.c) und die Kachel zeigt die laufende Zeit bzw. "‖ h:mm:ss" in der Pause (P6 app_glance_service.c + Kacheldatei aus b25 als eigene Datei im K1-Wächter). Nur mit CONFIG_BOULDER_KIESELSPORT; k1_waechter um P5/P6 mit Zeilengrenzen erweitern; bisherige Glance-Lösung aus AP3 (app_glance_db) entfernen, falls sie nicht mehr gebraucht wird. Tests (die b25-Kacheltests portieren) und QEMU-Bild des Startmenüs im Training (Bild ansehen).
Dokumente nachziehen: ${ORT}/schritt2-spezifikation.md (Abschnitt 10 um die Entscheide vom 08.10. ergänzen, K1-Liste mit P5/P6 und den drei alarm.c-Stellen), ${ORT}/schritt2-abnahme.md (K1, K4, K5 ohne Vergleich mit echten Trainings als erfüllt mit Host-Tests, K6 mit Startmenü), ${ORT}/schritt1-spezifikation.md (E4: drei Funktionen, ≤ 20 Zeilen, Entscheid 08.10.).`, { label: 'nachtrag-bau', phase: 'Bau', schema: ERG })

phase('Prüfung')
const pruefung = await agent(`${GRUND}

Gegenprüfung (nichts ändern): Prüfe die Commits ${bau ? bau.commits.join(', ') : '(jüngste auf origin/v2)'} gegen die Entscheide 2 und 3. Zählen Schritte in der Pause wirklich (auch Minute/Rate), und während des Laufs nicht? Kein Doppelzählen beim Wechsel Pause/Weiter? P5/P6 nur mit CONFIG, andere Bretter mit Pebbles Workout-App unverändert, Kachel richtig bei Pause/Weiter/Ende/Verwerfen und nach Neustart? K1-Wächter-Grenzen eng? Belege jeden Befund.`, { label: 'nachtrag-pruefung', phase: 'Prüfung', schema: BEFUNDE, effort: 'high' })
let nach = null
if (pruefung && pruefung.befunde.length) {
  nach = await agent(`${GRUND}

Behebe diese Befunde (jeden zuerst prüfen; falsche begründet ablehnen), Tests + Mutation, commit + push:
${JSON.stringify(pruefung.befunde, null, 2)}`, { label: 'nachtrag-nachbesserung', phase: 'Prüfung', schema: ERG })
}
return { bau, pruefung, nach }
