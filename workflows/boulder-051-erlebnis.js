export const meta = {
  name: 'boulder-051-erlebnis',
  description: 'Boulder 0.51.0: neues Health-Erlebnis (Tagesband, Sport-Feld, Stack, Heart & Body, Schlaf) bauen, prüfen, Release vorbereiten',
  phases: [
    { title: 'Karte', detail: 'Code-Landkarte der betroffenen Stellen' },
    { title: 'Bau', detail: 'Pakete A bis D nacheinander, je Commit und Push' },
    { title: 'Prüfung', detail: 'eine adversariale Gegenprüfung' },
    { title: 'Nachbesserung', detail: 'Befunde beheben' },
    { title: 'Abschluss', detail: 'Gesamttest, Release-Build, Notiz, PNGs' },
  ],
}

const SPEZ = '/home/dysseus/Documents/Claude/boulder-v1/tagesband-0.51-spezifikation.md'
const KARTE = '/home/dysseus/Documents/Claude/boulder-v1/karte-0.51.md'
const WT = '/home/dysseus/Projects/boulder-051'
const TRAILER = 'Commit-Nachrichten deutsch und mit diesen zwei Schlusszeilen:\nCo-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>\nClaude-Session: https://claude.ai/code/session_012RYr1ZRXBzb7Y9hLeMoBPc'
const GEMEINSAM = `Lies zuerst die Spezifikation ${SPEZ} vollständig und halte dich an ihre Grundregeln. Arbeitskopie: ${WT} (Zweig tagesband-0.51). Die Entwurfsflächen liegen als HTML unter ~/Documents/Claude/boulder-v1/entwurf-erlebnis/ – Masse, Farben und Anordnung dort ablesen. ${TRAILER}\nNach deinem Paket: committen und \`git push -u origin tagesband-0.51\`. Kein Push auf main, kein Tag, kein Release.`

const PAKET = {
  type: 'object',
  properties: {
    commits: { type: 'array', items: { type: 'string' } },
    tests: { type: 'string', description: 'welche Tests liefen, Ergebnis, Mutationen mit Ergebnis' },
    pngs: { type: 'array', items: { type: 'string' }, description: 'absolute Pfade der gezeichneten Bildschirme' },
    abweichungen: { type: 'array', items: { type: 'string' }, description: 'wo und warum vom Entwurf/der Spezifikation abgewichen' },
    offen: { type: 'array', items: { type: 'string' } },
  },
  required: ['commits', 'tests', 'pngs', 'abweichungen', 'offen'],
}

phase('Karte')
const karte = await agent(`${GEMEINSAM}

Aufgabe: Erstelle eine Code-Landkarte für die vier Pakete der Spezifikation, NOCH OHNE etwas zu ändern. Schreibe sie nach ${KARTE} (deutsch, knapp, mit Dateipfaden und Funktionsnamen) und gib eine Zusammenfassung zurück. Inhalt:
- Health-Start: Datei/Composable des Health-Tabs, Kopf mit Datum, bisheriger Zeitstrahl ("Today so far"), Herzzeile, Wasser-, Präparate-, Koffeinzeilen; woher Schlaf (Hypnogramm-Läufe), Puls (5-Minuten-Bins), Schritte je Intervall, Getränke, Gläser, Präparate (SupCycle), Koffeinmodell, Trainings, Schlafenszeit kommen; wie langer Druck zum Ändern/Löschen umgesetzt ist.
- Navigation: wo die untere Leiste und der Sport-Reiter definiert sind; ALLE Wege, die den Sport-Reiter öffnen (Benachrichtigungen, Deep Links, Trainingsende, Widgets, Intents).
- Sportseite: Composable, Wochendefinition (THIS WEEK, Tagesreihe), letztes Training.
- Schlafansicht: Stages-Karte, Hypnogramm-Karte.
- Herzseite: Pulsband, HRV-Karte (wird HrvWoche schon benutzt?), Blutdruckkarte, Body-Zeile, wie das HRV-Band berechnet wird (Gesundheitsrechnung), Trendansicht und wie sie erreichbar ist.
- Wasser-, Präparate-, Koffein-Detailseiten falls vorhanden.
- Bildtests (Robolectric NATIVE wie PulsKarteBildTest): wie man einen ganzen Bildschirm mit Beispieldaten zeichnet.
- Versionsstelle (boulderVersionName) und Muster der Release-Notizen (.github/release/).`, { label: 'karte', phase: 'Karte' })

phase('Bau')
const PAKETE = [
  { key: 'A', text: 'Paket A – Health-Start: Datumszeile weg, Tagesband statt Zeitstrahl, Sport-Feld, Liste (Heart & Body umbenannt, Water, Stack), Navigation ohne Sport-Reiter mit Sport als Unterseite inkl. aller Einstiege. Bildbelege: Health-Start (Band beim Öffnen), dazu das ganze Band einmal ungeschnitten, Sportseite mit Zurück-Pfeil.' },
  { key: 'B', text: 'Paket B – Schlafansicht: Stages-Karte weg, Phasenzeiten unten in der Hypnogramm-Karte. Bildbeleg: Schlafansicht.' },
  { key: 'C', text: 'Paket C – Heart & Body: Titel, HRV als HrvWoche (prüfen/anpassen), Blutdruck im selben Stil mit eigenen Bändern für oben/unten (Methode wie HRV-Band), Trends-Zeile am Ende. Bildbeleg: ganze Seite.' },
  { key: 'D', text: 'Paket D – Water und Stack: Wasserseite anpassen oder neu, Stack-Seite neu (Präparate + Koffeinkurve aus dem bestehenden Modell). Bildbelege: beide Seiten.' },
]
const ergebnisse = []
for (const p of PAKETE) {
  const vorher = ergebnisse.map(e => `Paket ${e.key}: Commits ${e.r ? e.r.commits.join(', ') : '–'}; offen: ${e.r ? e.r.offen.join(' | ') : 'Agent ausgefallen'}`).join('\n')
  const r = await agent(`${GEMEINSAM}

Die Code-Landkarte liegt in ${KARTE}. Bisherige Pakete:
${vorher || '(keine)'}

${p.text}

Vorgehen: umsetzen, passende Tests schreiben (Kernaussagen mit Mutation belegen), betroffene Modul-Tests laufen lassen, Bildbelege als PNG zeichnen und die Pfade melden. Weiche nur ab, wo der Code es erzwingt, und begründe jede Abweichung.`, { label: `paket-${p.key}`, phase: 'Bau', schema: PAKET })
  ergebnisse.push({ key: p.key, r })
  log(`Paket ${p.key}: ${r ? r.commits.length + ' Commits, ' + r.pngs.length + ' Bilder' : 'ausgefallen'}`)
}

phase('Prüfung')
const BEFUNDE = {
  type: 'object',
  properties: {
    befunde: {
      type: 'array',
      items: {
        type: 'object',
        properties: {
          datei: { type: 'string' }, zeile: { type: 'integer' },
          schwere: { type: 'string', enum: ['hoch', 'mittel', 'niedrig'] },
          beschreibung: { type: 'string' }, beleg: { type: 'string' },
        },
        required: ['datei', 'schwere', 'beschreibung', 'beleg'],
      },
    },
  },
  required: ['befunde'],
}
const pruefung = await agent(`${GEMEINSAM}

Du bist die Gegenprüfung (nichts ändern, nichts committen). Prüfe den Diff \`git -C ${WT} diff main...tagesband-0.51\` gegen die Spezifikation und die Entwürfe. Suche gezielt nach echten Fehlern und belege jeden mit Datei/Zeile und einem nachvollziehbaren Beleg (Testlauf, Codepfad, Bild):
- Navigation: führt noch irgendein Weg ins Leere oder auf den entfernten Reiter? Zurück-Taste von Sport?
- Langer Druck zum Ändern/Löschen auf Band-Symbolen und Listen: funktioniert wie vorher?
- Tagesband: Zeitfenster-Regel, Hypnogramm-Lage, Puls-Lücken, Jetzt-Puls nur ≤15 min, Zusammenfassen/Schieben dichter Einträge, Verhalten ohne Nacht / ohne Daten / ohne Drinktervall-SupCycle.
- Stack und Koffein: Werte stimmen mit dem bestehenden Modell überein (nicht neu gerechnet)?
- Blutdruck-Bänder: gleiche Methode wie HRV, neutral ohne genug Messungen?
- Grundregeln: nur Englisch in der UI, keine Erklärtexte, Baukasten/Farben/Formate benutzt, kein catch ohne Log, Tests bauen Produktionscode nicht nach, BLE/Benachrichtigungen unberührt.
- Sieh dir die gemeldeten PNGs an (Read-Werkzeug) und vergleiche mit den Entwürfen.
Bildpfade der Pakete: ${ergebnisse.map(e => e.r ? e.r.pngs.join(', ') : '').join(', ')}
Melde nur Befunde, die du belegen kannst.`, { label: 'pruefung', phase: 'Prüfung', schema: BEFUNDE, effort: 'high' })

phase('Nachbesserung')
let nachbesserung = null
const befunde = pruefung ? pruefung.befunde.filter(b => b.schwere !== 'niedrig' || true) : []
if (befunde.length) {
  nachbesserung = await agent(`${GEMEINSAM}

Behebe diese Befunde der Gegenprüfung. Prüfe jeden zuerst selbst; ist einer falsch, begründe das statt zu ändern. Tests ergänzen, wo ein Befund eine Lücke zeigte. Danach betroffene Tests laufen lassen, Bilder neu zeichnen, wenn sich ein Bildschirm änderte.

${JSON.stringify(befunde, null, 2)}`, { label: 'nachbesserung', phase: 'Nachbesserung', schema: PAKET })
} else {
  log('Keine Befunde – Nachbesserung entfällt')
}

phase('Abschluss')
const abschluss = await agent(`${GEMEINSAM}

Abschluss für 0.51.0 auf dem Zweig (nicht auf main):
1. Version auf 0.51.0 setzen (Muster der früheren Versionen).
2. Release-Notiz .github/release/0.51.0.md schreiben: deutsch, erste Zeile Titel, kurz; jeder Satz muss durch Code/Test belegt sein.
3. ci/debug.keystore nach $ANDROID_USER_HOME/debug.keystore kopieren (Ordner anlegen falls nötig), dann den GESAMTTEST aus der Spezifikation laufen lassen. Bekannte Fehlschläge, die schon auf main bestehen, gesondert nennen (auf main gegenprüfen).
4. Die Release-APK mit apksigner prüfen: Zertifikat muss mit 212e0ca4 beginnen. Pfad und SHA-256 der APK melden.
5. Alle Bildbelege neu zeichnen und die Pfade melden.
6. Committen und pushen.
Gib in 'tests' den Gesamttest mit Ergebnis an, in 'offen' alles, was der Hauptagent vor dem Release wissen muss (inkl. APK-Pfad, Zertifikat, Hash).`, { label: 'abschluss', phase: 'Abschluss', schema: PAKET })

return { karte, ergebnisse, pruefung, nachbesserung, abschluss }
