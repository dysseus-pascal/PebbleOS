export const meta = {
  name: 'review2-abschluss',
  description: 'Firmware-Review-Fixes abschliessen, SupCycle-Review-Fixes neu (nach Neustart verloren), Absturz-Kandidaten gegenpruefen',
  phases: [
    { title: 'Abschluss', detail: 'fw, supcycle, absturz' },
  ],
}

const S = '/tmp/claude-1000/-home-dysseus-Documents-Claude/a0d87cde-04f6-43b4-bc17-a2150aed9501/scratchpad'
const SKRIPT1 = '/home/dysseus/.claude/projects/-home-dysseus-Documents-Claude/a0d87cde-04f6-43b4-bc17-a2150aed9501/workflows/scripts/audit-umsetzen-welle1-wf_15a134f4-482.js'
const SKRIPT_R2 = '/home/dysseus/.claude/projects/-home-dysseus-Documents-Claude/a0d87cde-04f6-43b4-bc17-a2150aed9501/workflows/scripts/review2-fixes-wf_8df0e390-3db.js'
const ABSTURZ_JOURNAL = '/home/dysseus/.claude/projects/-home-dysseus-Documents-Claude/a0d87cde-04f6-43b4-bc17-a2150aed9501/subagents/workflows/wf_dca93257-afe/journal.jsonl'
const HINWEIS = `HINWEIS: Der Rechner wurde neu gestartet, /tmp (tmpfs) wurde dabei geleert. Fruehere Scratch-Dateien und Worktrees unter ${S} sind weg; committete Arbeit liegt in den Branches. ${S}/review-welle2.json ist neu erzeugt.`
const REGELN = `Arbeits-, Code- und Testregeln: ${SKRIPT1}, Konstante REGELN, dazu die Umgebung FW bzw. UHRAPP. Halte sie ein (Commits mit Trailer, kein Push, kein Merge nach main, keine Versionserhoehung, keine Emulatoren, Mutation je Fix mit Ergebnis im Bericht, Tests kopieren keinen Produktionscode, kein catch ohne Log, Firmware-Submodule fuer Builds per Symlink aus ~/Projects/pebbleos einhaengen und am Ende aushaengen).
BERICHT: je Punkt Stand, Dateien, Test, Mutation, Restrisiko; Branch, Worktree, letzter Commit; vollstaendige Testliste. Deutsch, knapp.`

const FW = `${HINWEIS}
FORTSETZUNG. Auftrag: ${SKRIPT_R2}, Eintrag key 'fw' in AUFTRAEGE (7 Punkte zu ${S}/review-welle2.json bereiche "fw-fork" und "fw-fixes").
Der vorige Agent hat auf Branch review2-fw 7 Commits gemacht (Markierungen, schalterlos-Kommentar, SpO2 Disabled, Kurvenstueck unter Sperre, Rueckweg b25->b24->b25, App-Release-Pruefung in release.sh, bundle.sh frisch + Schalterpruefung), wurde aber vor Testlauf, Mutationsnachweisen und Bericht gestoppt.
WORKTREE: git -C ~/Projects/pebbleos worktree add ${S}/wt/fw-r2 review2-fw (bestehenden Branch auschecken).
Vorgehen: jeden der 7 Punkte am Code pruefen (fehlt etwas: nachholen), Mutation je Fix belegen, alle Host-Tests (beide Schalter-Modi, ctest --timeout 300), obelix-Release-Bau ueber den bundle.sh-Weg (Slot 0 und 1; dafuer darf build-boulder/ im Worktree entstehen), Bericht fuer alle 7 Punkte.`

const SC = `${HINWEIS}
NEU (die Arbeit des vorigen Agenten ging unkommittiert verloren). Auftrag: ${SKRIPT_R2}, Eintrag key 'supcycle' in AUFTRAEGE (6 Punkte zu ${S}/review-welle2.json bereich "supcycle", ohne den Versionsbefund).
WORKTREE: git -C ~/Projects/SupCycle worktree add ${S}/wt/sc-r2 review2-sc (Branch steht auf main). node_modules aus ~/Projects/SupCycle kopieren; Clay-dist mit python -m zipfile -e node_modules/@rebble/clay/dist.zip node_modules/@rebble/clay/dist entpacken, falls dist/ fehlt.
WICHTIG: Committe nach jedem fertigen Punkt (mit Test), damit nichts mehr verloren geht.`

const AB = `${HINWEIS}
Du bist Skeptiker und Gutachter. Die Pebble-Uhr des Nutzers (emery/obelix, Boulder-Firmware, b23 oder b24) ist am 2026-10-05 genau um 06:00 zur Weckzeit abgestuerzt; am 04.10. morgens hing sie schon einmal. Zwei Sucher haben Kandidaten geliefert; ihre Ergebnisse stehen als JSON in ${ABSTURZ_JOURNAL} (Zeilen mit "type":"result"). Ein dritter Sucher (QEMU) wurde durch den Neustart abgebrochen; der zweite Sucher hat QEMU-Laeufe von b23/b24 mit Wecker bereits gemacht (kein Absturz).
Seither in ~/Projects/pebbleos main uebernommen: Upstream 5d7effdc5 (Analytics-Heartbeat asserted bei vollem DLS-Pool -> stuendlicher Neustart, sich selbst erhaltend), 7ebe32f12 (heap_realloc Kopiergroesse), 91103e178 (Schlafzustand nach Neustart), 9f429c37f war schon enthalten. Ausserdem im neuen Stand: Watchdog-Futter im Morgenpfad, health_sync per Timer, Heap-Pruefung, eigene DLS-Tags 0x4001/0x4002.
Aufgabe (nur lesen, Proben nur unter ${S}/absturz2/): (1) Pruefe die Kandidaten mit hoher/mittlerer Wahrscheinlichkeit am Code der Tags v4.38.4-b23 und v4.38.4-b24 und versuche sie zu widerlegen. (2) Pruefe besonders den Analytics-Kandidaten: Kann der DLS-Pool (20 Plaetze) bei Boulder voll werden? Zaehle alle DLS-Sitzungen, die b23/b24 anlegen (System-Tags 81/83/84/85/87/88/89, Kieselsport, Watchapps wie Drinktervall/SupCycle?, Heartbeat je Boot), wie sie nach Neustarts wiederhergestellt und nie wiederverwendet werden, und ob ein Absturz genau um 06:00 dazu passt (Heartbeat stuendlich ab Boot: passt 06:00 nur, wenn ein Boot um x:00 war). (3) Ordne alle Kandidaten neu, sage je Kandidat, ob er im aktuellen main behoben ist, und welche Zeichen im Coredump/Flash-Log ihn bestaetigen (RebootReason, Assert-Datei/Zeile, Task). (4) Gib eine kurze Anleitung, wie der Nutzer die Flash-Logs aus dem Boulder-Fehlerbericht (Teilen-Symbol, ZIP) bereitstellen kann und wie ich sie mit tools/dehash_flash_logs.py bzw. analyze_coredump.py und einem ELF des passenden Tags auswerte (welches ELF, welche Befehle). Deutsch, knapp.`

phase('Abschluss')
const r = await parallel([
  () => agent(`${FW}\n\n${REGELN}`, { label: 'abschluss:fw', phase: 'Abschluss' }).then(b => ({ bereich: 'fw-r2', bericht: b })),
  () => agent(`${SC}\n\n${REGELN}`, { label: 'abschluss:supcycle', phase: 'Abschluss' }).then(b => ({ bereich: 'sc-r2', bericht: b })),
  () => agent(AB, { label: 'abschluss:absturz', phase: 'Abschluss' }).then(b => ({ bereich: 'absturz', bericht: b })),
])
return r.filter(Boolean)
