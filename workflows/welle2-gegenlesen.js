export const meta = {
  name: 'welle2-gegenlesen',
  description: 'Adversariales Review des gemergten Codes der zweiten Welle und der Review-Fixes, Befunde unabhaengig gegenpruefen',
  phases: [
    { title: 'Lesen', detail: 'sechs Pruefer, nur lesen' },
    { title: 'Gegenpruefen', detail: 'ein Skeptiker je Bereich' },
  ],
}

const S = '/tmp/claude-1000/-home-dysseus-Documents-Claude/a0d87cde-04f6-43b4-bc17-a2150aed9501/scratchpad'
const GRUND = `Nur lesen: in ~/Projects nichts aendern, nichts committen. Kleine Proben nur unter ${S}/review2/<bereich>/ (bei Bedarf git worktree dort, am Ende entfernen; Firmware-Submodule fuer Builds per Symlink aus ~/Projects/pebbleos einhaengen und wieder aushaengen). Keine Emulatoren (QEMU/AVD) starten; parallel laeuft eine Absturz-Analyse mit QEMU.
Projektregeln: deutsche Bezeichner fuer neuen eigenen Code; Kommentare erklaeren das WARUM; Eingriffe in Upstream mit "Boulder:"; UI-Texte in Boulder nur Englisch, keine Erklaer-Textwaende; BLE-Verbindung/Reconnect/Benachrichtigungen unveraendert; Health-Connect-Daten des Nutzers nie loeschen ausser gezielt eigene; Nutzerdaten (DB-Kopien) nie loeschen; kein catch ohne Log; Tests kopieren keinen Produktionscode; nichts still weggelassen.
NUTZERENTSCHEIDUNGEN: Feature-Flags Firmware (alle an = Verhalten wie vorher, alle aus = Upstream), Health-App nur emery (sonst Pebbles), eigene DLS-Tags 0x4001/0x4002 (App nimmt 88/89 weiter), eigene Room-DB fuer Boulder-Tabellen mit Datenumzug, Prefs-Platz (3072), Mindest-App-Zeile. Nacht = ein Schlafblock, Schlafsumme ohne Wachminuten. Kieselstrasse summt auch bei gleicher Abbiegung. SupCycle echte Kalendertage, Kur ohne Pause endet. Drinktervall still nach Ziel. Nicht gewaehlt (muss unveraendert sein): Dev-LAN-Server, Cohorts-CAs, Signatur, Kartenlink-Rueckfrage, OsmAnd-Signatur, Stock-Firmware-Unterstuetzung, Sicherung verschluesseln, Kieselsport mehrere Trainings puffern / nach Neustart retten.
RELEASE-REIHENFOLGE: Boulder 0.50.0 zuerst, dann Firmware b25 mit "Mindest-App: 0.50.0". Pruefe auch Kompatibilitaet alter/neuer Fassungen (App 0.49 + neue Uhr, neue App + b24, Rueckweg).
Suche: (a) Berichtsaussagen, die am Code nicht stimmen; (b) neue Fehler (Logik, Grenzfaelle, Nebenlaeufigkeit, Speicher, Zeitzonen, Kompatibilitaet alter Daten/Fassungen, Datenverlust); (c) Tests, die nichts pruefen oder Produktionscode kopieren; (d) Regelverstoesse. Jeder Befund mit Datei:Zeile, konkretem Ablauf, Schwere (hoch/mittel/niedrig) und Fix-Vorschlag. Keine folgenlosen Stilfragen.`

const BEFUNDE = {
  type: 'object',
  properties: {
    befunde: { type: 'array', items: { type: 'object', properties: {
      titel: { type: 'string' }, ort: { type: 'string' }, ablauf: { type: 'string' },
      schwere: { type: 'string', enum: ['hoch', 'mittel', 'niedrig'] }, art: { type: 'string', enum: ['aussage-falsch', 'neuer-fehler', 'test-leer', 'regel'] },
      fix: { type: 'string' },
    }, required: ['titel', 'ort', 'ablauf', 'schwere', 'art', 'fix'] } },
  },
  required: ['befunde'],
}
const URTEIL = {
  type: 'object',
  properties: { urteile: { type: 'array', items: { type: 'object', properties: {
    titel: { type: 'string' }, echt: { type: 'boolean' }, begruendung: { type: 'string' }, schwere: { type: 'string', enum: ['hoch', 'mittel', 'niedrig'] },
  }, required: ['titel', 'echt', 'begruendung'] } } },
  required: ['urteile'],
}

const BEREICHE = [
  { key: 'fw-fork', text: `Firmware ~/Projects/pebbleos: Diff 8199d62c2..main, Commits des Fork-Umbaus (Kconfig-Schalter, haken.h, boulder_health-Aufteilung, DLS-Tags 0x4001/0x4002, vertraege.c/.py, shellpref 3072, memmove, ANKUNFT uint32, Mindest-App in release.sh, BOULDER.md). Bericht: ${S}/bericht-f3-fork.md. Besonders: Verhalten mit allen Schaltern an wirklich gleich wie vorher; Upstream-Health bei Schalter aus vollstaendig; Kompatibilitaet Flash-Reste 88/89 und Prefs ueber das Update; release.sh-Pruefung.` },
  { key: 'fw-fixes', text: `Firmware ~/Projects/pebbleos: Diff 8199d62c2..main, Commits der Review-Fixes (b24-Update am Vormittag, Sitzungen vor 'erledigt' speichern, letzter Platz genullt, Kommentare, TimeStyle-Zeiger nach Absturz, Kieselstrasse Lage der Abzweigung, Kieselsport-Sperren). Bericht: ${S}/bericht-fw-review.md. Besonders: activity_sessions_prv_speichern unter welcher Sperre und auf welchem Task, Flash-Last, neue Erkennung in weg.c bei Rundungen/Umleitungen/Navigationsende, Deadlock-Gefahr durch ks_sperren auf dem App-Task.` },
  { key: 'boulder-db', text: `Boulder ~/Projects/Boulder: Diff 73039f4b..main fuer libpebble3/ (BoulderDatenbank, NachtUmzug, UmgezogenerNachtDao, DowngradeSchutz 1045, BoulderDls, Parser, Schemata) und pebble/.../firmware (MindestApp, GithubReleases, Firmware-Hinweis). Berichte: ${S}/bericht-a3-fork.md, ${S}/bericht-lp-r.md. Besonders: Datenerhalt beim Umzug, Verhalten bei gescheitertem Umzug, Downgrade-Schutz fuer beide DBs, Room-Migrationen 45..50->1045, 88/89 vs. 0x4001/0x4002, Mindest-App-Regex und Anzeige.` },
  { key: 'boulder-fixes', text: `Boulder ~/Projects/Boulder: Diff 73039f4b..main ausser den Datenbank-Dateien: Kiesel-Fixes (Kieselsport Erfolg getrennt, Baufehler-Rueckfall, Kieselstrasse Folgenummer/Abo, Sicherung Fortschritt, Zonen, deutsche Bezeichner) und libpebble3-Fixes (Backup-Ausschluss, Begleiter-Kanal, NACK, Quarantaene-fsync/Hysterese, RemoteTimelineEmulator, Nacht-Aufrufer, Mindest-App-Anzeige). Berichte: ${S}/bericht-kiesel-r.md, ${S}/bericht-lp-r.md.` },
  { key: 'supcycle', text: `SupCycle ~/Projects/SupCycle: Diff zwischen dem Merge "Merge branch 'audit-w1'" und main (Aufschub-Raender w1b, Review-Fixes sc-r: CI, offenes Fenster, lange Pause, 03:00-Wecker, Persist-Faecher). Berichte: ${S}/bericht-w1b.md, ${S}/bericht-sc-r.md. Besonders die bewusste Abweichung bei Punkt 4 (03:00-Wecker) und "Spaeter gilt nur der neuesten Runde".` },
  { key: 'drinktervall', text: `Drinktervall ~/Projects/Drinktervall: Diff 6cc07ac..main (CI-Fix, N7 mit Animation, drinktervall_verlassen, Attrappen-Aenderungen). Bericht: ${S}/bericht-dt-r.md.` },
]

const ergebnisse = await pipeline(
  BEREICHE,
  b => agent(`Du bist ein strenger Code-Reviewer. Bereich: ${b.text}\n\n${GRUND}\n\nDeutsch, knapp.`, { label: `lesen:${b.key}`, phase: 'Lesen', schema: BEFUNDE }),
  (r, b) => {
    if (!r || !r.befunde || r.befunde.length === 0) return { bereich: b.key, befunde: [], urteile: [] }
    return agent(`Du bist ein Skeptiker. Ein Reviewer hat im Bereich "${b.key}" folgende Befunde gemeldet. Versuche jeden am Code zu WIDERLEGEN. Echt nur, wenn der Ablauf am aktuellen Code wirklich zum Fehler fuehrt bzw. Aussage/Regel wirklich verletzt ist. Bei Unsicherheit nachstellen (Probe unter ${S}/review2/verify-${b.key}/), sonst echt=false mit Grund. Schwere ggf. korrigieren. Antworte mit genau einem Urteil je Befund, Titel unveraendert uebernehmen.\n\nBereich: ${b.text}\n\nBefunde:\n${JSON.stringify(r.befunde, null, 1)}\n\nNur lesen, keine Emulatoren. Deutsch, knapp.`,
      { label: `gegen:${b.key}`, phase: 'Gegenpruefen', schema: URTEIL })
      .then(u => ({ bereich: b.key, befunde: r.befunde, urteile: u ? u.urteile : [] }))
  },
)
return ergebnisse.filter(Boolean)
