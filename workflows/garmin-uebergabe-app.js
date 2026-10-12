export const meta = {
  name: 'garmin-uebergabe-app',
  description: 'Kleine Android-App: Garmin-Export (Gewicht, Zusammensetzung, Blutdruck) einmalig rückwirkend nach Health Connect schreiben',
  phases: [
    { title: 'Bau', detail: 'Projekt, Parser, Abgleich, UI, Tests, APK' },
    { title: 'Prüfung', detail: 'Gegenprüfung Datenschutz und Datenrichtigkeit' },
    { title: 'Nachbesserung', detail: 'Befunde beheben, APK neu' },
  ],
}

const SPEZ = '/home/dysseus/Documents/Claude/boulder-v1/garmin-uebergabe-spezifikation.md'
const PROJ = '/home/dysseus/Projects/Garmin-Uebergabe'
const GRUND = `Lies zuerst ${SPEZ} vollständig. Projekt: ${PROJ} (neu anlegen, git init, lokal committen; KEIN GitHub-Repo anlegen, nichts pushen). Umgebung: JAVA_HOME=/usr/lib/jvm/java-25-openjdk ANDROID_HOME=/opt/android-sdk ANDROID_USER_HOME=/tmp/claude-1000/-home-dysseus-Documents-Claude/a0d87cde-04f6-43b4-bc17-a2150aed9501/scratchpad/android-home. Gradle-Wrapper und Versionen nach Vorbild ~/Projects/Boulder (dort nur lesen). Bezeichner deutsch, Kommentare zum WARUM, UI-Texte Englisch, kein catch ohne Log. Persönliche Gesundheitsdaten aus dem Scratchpad NIE ins Repository, nie in Tests, nie ins Netz. Commit-Nachrichten deutsch mit den Schlusszeilen:
Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_012RYr1ZRXBzb7Y9hLeMoBPc`

const ERG = { type: 'object', properties: {
  commits: { type: 'array', items: { type: 'string' } }, tests: { type: 'string' },
  selbsttest: { type: 'string', description: 'was der Parser aus dem echten Export liest: Anzahl je Art, Zeitraum' },
  apk: { type: 'string', description: 'Pfad, Grösse, SHA-256, Zertifikat, versionName' },
  pngs: { type: 'array', items: { type: 'string' } },
  abweichungen: { type: 'array', items: { type: 'string' } }, offen: { type: 'array', items: { type: 'string' } } },
  required: ['commits', 'tests', 'selbsttest', 'apk', 'pngs', 'abweichungen', 'offen'] }

phase('Bau')
const bau = await agent(`${GRUND}

Baue die App nach der Spezifikation: Parser (ZIP über SAF/InputStream, beide Dateiarten, mehrere Blutdruckdateien), Abbildung auf die 5 Health-Connect-Arten, Abgleich ±10 min gegen Vorhandenes, stabile clientRecordId/Version, Vorschau und Schreiben in Blöcken, Ergebnis je Art, Rechte nur für die 5 Arten (lesen + schreiben), keine INTERNET-Berechtigung, Health-Connect-Rationale. Tests mit nachgebauten JSON-Dateien und Mutationen. Selbsttest: Parser lokal gegen den echten Export im Scratchpad laufen lassen (nur zählen, Ergebnis melden, nichts davon speichern). Release-APK mit ~/Projects/Boulder/ci/debug.keystore signieren (Passwörter wie in Boulders Build-Konfiguration), mit apksigner prüfen. PNG des Bildschirms (Robolectric) nach ~/Documents/Claude/boulder-v1/belege-garmin/.`, { label: 'garmin-bau', phase: 'Bau', schema: ERG })

phase('Prüfung')
const BEFUNDE = { type: 'object', properties: { befunde: { type: 'array', items: { type: 'object', properties: {
  datei: { type: 'string' }, schwere: { type: 'string', enum: ['hoch', 'mittel', 'niedrig'] }, beschreibung: { type: 'string' }, beleg: { type: 'string' } },
  required: ['datei', 'schwere', 'beschreibung', 'beleg'] } } }, required: ['befunde'] }
const pruefung = await agent(`${GRUND}

Gegenprüfung (nichts ändern): Prüfe ${PROJ} gegen die Spezifikation. Schwerpunkte: Kann die App etwas löschen oder überschreiben, das nicht von ihr stammt? Doppelte Einträge (zweiter Lauf, Garmins eigene Sätze, Zeitzone/Sommerzeit beim Blutdruck)? Einheiten (g→kg, % → Wassermasse, Fett %)? Quellenfilter (CHANGE_LOG/USER_SETTING raus)? Nullen raus? Rechte nur die 5 Arten, keine INTERNET-Berechtigung, keine persönlichen Daten im Repo (git log/ls-files prüfen)? Fehlerbehandlung beim Schreiben? Belege jeden Befund. Bild: ${bau ? bau.pngs.join(', ') : ''}`, { label: 'garmin-pruefung', phase: 'Prüfung', schema: BEFUNDE, effort: 'high' })

phase('Nachbesserung')
let nach = null
if (pruefung && pruefung.befunde.length) {
  nach = await agent(`${GRUND}

Behebe diese Befunde (jeden zuerst prüfen; falsche begründet ablehnen), Tests + Mutation, APK neu bauen und prüfen, PNG neu, committen:
${JSON.stringify(pruefung.befunde, null, 2)}`, { label: 'garmin-nachbesserung', phase: 'Nachbesserung', schema: ERG })
}
return { bau, pruefung, nach }
