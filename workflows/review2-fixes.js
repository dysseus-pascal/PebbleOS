export const meta = {
  name: 'review2-fixes',
  description: 'Bestaetigte Befunde aus dem Gegenlesen der zweiten Welle beheben (Firmware, Boulder, SupCycle, Drinktervall)',
  phases: [
    { title: 'Beheben', detail: 'vier Agenten, je eigener Worktree' },
  ],
}

const S = '/tmp/claude-1000/-home-dysseus-Documents-Claude/a0d87cde-04f6-43b4-bc17-a2150aed9501/scratchpad'
const SKRIPT1 = '/home/dysseus/.claude/projects/-home-dysseus-Documents-Claude/a0d87cde-04f6-43b4-bc17-a2150aed9501/workflows/scripts/audit-umsetzen-welle1-wf_15a134f4-482.js'
const REGELN = `Arbeits-, Code- und Testregeln: ${SKRIPT1}, Konstante REGELN, dazu die Umgebung FW, BOULDER bzw. UHRAPP. Halte sie ein (eigener Worktree, Commits mit Trailer, kein Push, kein Merge nach main, keine Versionserhoehung, keine Emulatoren, Mutation je Fix mit Ergebnis im Bericht, Tests kopieren keinen Produktionscode, kein catch ohne Log, Firmware-Submodule fuer Builds per Symlink aus ~/Projects/pebbleos einhaengen und am Ende aushaengen).
Befunde mit Ablauf, Fix-Vorschlag und Skeptiker-Urteil: ${S}/review-welle2.json (Liste je bereich; urteile[i] gehoert zu befunde[i]). Zu beheben sind nur Befunde mit echt=true (bzw. die unten genannten); jeden am Code nachpruefen. Berichtskorrekturen ohne Code nur im Bericht nennen.
BERICHT: je Punkt Stand, Dateien, Test, Mutation, Restrisiko; Branch, Worktree, letzter Commit; vollstaendige Testliste. Deutsch, knapp.`

const AUFTRAEGE = [
  { key: 'fw', text: `Firmware ~/Projects/pebbleos. WORKTREE: git -C ~/Projects/pebbleos worktree add ${S}/wt/fw-r2 -b review2-fw main
bereiche "fw-fork" und "fw-fixes" (echt=true):
1. Mindest-App: release.sh prueft vor dem Tag, dass das App-Release v$MINDEST_APP im Boulder-Repo veroeffentlicht und kein Entwurf ist (gh release view, Repo aus BOULDER.md bzw. dysseus-pascal/Boulder pruefen); sonst Abbruch. BOULDER.md und Docstring mindest_app.py richtigstellen: App <= 0.49 wertet die Zeile nicht aus; das Telefon muss vor b25 auf >= 0.50 stehen. CRLF-Begruendung richtigstellen. Test (test_boulder_release_mindest_app) mit gefaelschtem gh.
2. bundle.sh: vor configure CMake-Cache/menuconfig entfernen oder --fresh; nach configure fuer obelix pruefen, dass CONFIG_BOULDER_SCHLAF/_HRV/_MESSSTUFE/_HEALTH_APP=y, sonst Abbruch. BOULDER.md: Probebau nur mit -b. Test soweit moeglich.
3. SpO2 bei Intervall Disabled: Verhalten behalten, in BOULDER.md als Angleichung an activity.c nennen, Testfall in test_health_daten.
4. "Boulder:"-Markierung in src/fw/services/Kconfig und services/CMakeLists.txt.
5. Rueckweg b25->b24->b25: alter Nacht-Zustand verhindert das Erkennen des b24-Bilds (Schlaf doppelt, Nacht zweimal gesendet). Fix + Test.
6. Kurvenstueck wird ohne Sperre gelesen (Werte von Training B als Kurve von A): unter ks_sperren lesen oder beginn pruefen. Test.
7. Das Nullen beim Zuruecknehmen in activity_sessions.c wirkt auch im Upstream-Bau: im Kommentar als bewusst schalterlos (Upstream-Fehler) dokumentieren.
Danach alle Host-Tests (beide Schalter-Modi) und obelix-Release-Bau mit bundle.sh-Weg (Slot 0 und 1).` },
  { key: 'boulder', text: `Boulder ~/Projects/Boulder. WORKTREE: git -C ~/Projects/Boulder worktree add ${S}/wt/boulder-r2 -b review2-boulder main (Submodul holen; local.properties fuer signierten Bau kopieren, nie committen).
bereiche "boulder-db" und "boulder-fixes" (echt=true):
1. Mindest-App: Ist die neueste Firmware fuer diese App zu neu, wird die neueste PASSENDE (Mindest-App erfuellt) angeboten; das neutrale Label nur, wenn keine passt. Eine Uhr im Recovery-Modus bekommt immer die neueste passende. Tests mit Mutation.
2. Vorwaertsvertrag 0x4001/0x4002: groessere/neuere Satzaufbauten werden nicht falsch gelesen, sondern bekannter Teil gelesen nur wenn die Fassung bekannt ist, sonst Quarantaene + ACK + Log (Entscheidung am Befund pruefen). Test.
3. Waechtertest des UmgezogenerNachtDao prueft das Warten auf den Umzug, nicht nur das Durchreichen. Mutation (Warten entfernen) muss rot werden.
4. echteTelefonKopie: ohne Umgebungsvariable als uebersprungen markieren (assumeTrue o. ae.), nicht gruen.
5. Backup-Ausschluss auch fuer API 28-30 (full_backup_content bzw. die passende Regeldatei fuer aeltere Android-Fassungen), Bericht korrigieren. Test.
6. Neue eigene Bezeichner deutsch (siehe Befund).
7. In CLAUDE.md bzw. Doku festhalten: Rueckweg 0.50 -> 0.49 leert in 0.49 die libpebble3.db (alter Code); Naechte bleiben in boulder.db. Nichts am Code noetig.
Voller Boulder-Testlauf und signierter Release-Bau am Ende.` },
  { key: 'supcycle', text: `SupCycle ~/Projects/SupCycle. WORKTREE: git -C ~/Projects/SupCycle worktree add ${S}/wt/sc-r2 -b review2-sc main (node_modules kopieren; Clay-dist ggf. mit python -m zipfile -e entpacken).
bereich "supcycle" (echt=true), AUSSER dem Versionsbefund (macht der Hauptagent beim Release):
1. Aufschub ueber Mitternacht zeigt schon Genommenes wieder an. Fix + Test.
2. 'Spaeter' im zusammengefuehrten Fenster laesst aeltere Runden still fallen: alle gezeigten Runden muessen wiederkommen (oder die aelteste behalten). Fix + Test.
3. Westreise + uebrig gebliebener Aufschubzustand: heutige Runde gilt als die von gestern, Genommen hakt nichts ab. Fix + Test.
4. 'Kurz davor': das Neustellen darf den Wecker nicht wiederholt bis zu 31 s schieben. Fix + Test.
5. Neuplan-Wecker bei leerem Plan oder beendeter Kur nicht jede Nacht stellen. Fix + Test.
6. Zurueck waehrend des Wartens auf das Telefon verwirft die vorgemerkte Runde und die App bleibt offen. Fix + Test.
Danach tools/alle_tests.sh und pebble build.` },
  { key: 'drinktervall', text: `Drinktervall ~/Projects/Drinktervall. WORKTREE: git -C ~/Projects/Drinktervall worktree add ${S}/wt/dt-r2 -b review2-dt main (node_modules kopieren; Clay-dist ggf. entpacken).
bereich "drinktervall" (echt=true):
1. Die Attrappe muss wie die Firmware entladen (pruefe applib/ui/window_stack.c in ~/Projects/pebbleos genau: wann wird unload bei window_stack_remove mit/ohne Animation aufgerufen). Danach N7-Fix und drinktervall_verlassen gegen die korrigierte Attrappe neu pruefen und ggf. anpassen; die Berichtsaussagen zu den Mutationen D/F neu belegen.
2. Restrisiko-Text "Wecker in den rund 250 ms" richtigstellen bzw. entfernen, wenn es das auf der Uhr nicht gibt.
3. Pin-Start mit dazwischenkommender Erinnerung: nach Zurueck oder "Enjoy!" muss die App sich schliessen. Fix + Test.
Danach alle Tests und pebble build.` },
]

phase('Beheben')
const r = await parallel(AUFTRAEGE.map(a => () =>
  agent(`${a.text}\n\n${REGELN}`, { label: `beheben:${a.key}`, phase: 'Beheben' }).then(b => ({ bereich: a.key, bericht: b }))))
return r.filter(Boolean)
