export const meta = {
  name: 'v2-schritt1-rest',
  description: 'V2 Schritt 1 fertigbauen (schlank, sequenziell): AP1, AP3, AP5, AP7, AP4, AP8, AP6, AP9, eine Gegenpruefung, AP10 Abnahmebericht',
  phases: [
    { title: 'Pakete', detail: 'je ein Agent, nacheinander auf Branch v2' },
    { title: 'Pruefung', detail: 'eine Gegenpruefung der riskanten Teile' },
    { title: 'Abnahme', detail: 'Befunde beheben, Abnahmebericht' },
  ],
}

const S = '/tmp/claude-1000/-home-dysseus-Documents-Claude/a0d87cde-04f6-43b4-bc17-a2150aed9501/scratchpad'
const V2 = '/home/dysseus/Documents/Claude/boulder-v2'
const WT = S + '/v2s1/wt-basis'
const GRUND = `Boulder V2, Schritt 1 "eigenes Kraepelin". SCHLANK arbeiten (Nutzer achtet auf Token-Verbrauch): keine Nebenrecherchen, keine langen Dokumente, Mutation nur fuer Kernaussagen, ein voller Testlauf am Ende des Pakets, Bericht hoechstens 20 Zeilen.
VERBINDLICH: ${V2}/entscheidungen.md (inkl. "Entscheidungen zur Spezifikation Schritt 1" und den 8 Abnahmekriterien). Bauplan: ${V2}/schritt1-spezifikation.md (Entscheidungen haben Vorrang). Lies nur die Abschnitte, die dein Paket betreffen.
ORT: Worktree ${WT} auf Branch v2 (origin/v2). Zuerst: git -C ${WT} pull --ff-only origin v2 und kurz pruefen, was die vorigen Pakete gebaut haben (git log, Kopf der relevanten Dateien). Falls der Worktree fehlt (Neustart, /tmp geleert): git -C ~/Projects/pebbleos worktree prune; git -C ~/Projects/pebbleos worktree add ${WT} v2; git -C ${WT} pull --ff-only origin v2; Submodule mit 'git submodule update --init' und --reference auf ~/Projects/pebbleos/<pfad>.
Umgebung: . ~/pebbleos-sdk-0.1.8/env.sh && . ~/Projects/pebbleos/.venv/bin/activate; pbl aus dem Worktree (PYTHONPATH=${WT}/tools/libs/pbl-cli). Host-Tests: cmake -G Ninja -S tests -B <dir>; cmake --build <dir> -- -k 0; ctest --timeout 300 (Baseline: 13 Upstream-Fehler, keine neuen erlaubt). obelix@pvt Release-Bau ohne Warnung.
REGELN: Pebble-Code nur an den entschiedenen Stellen (Algorithmus-Wahl Kconfig/CMake, Messstufe-Whitelist, alarm.c Weckregel, hrm_manager.c HRV-Fix F2, settings/health.c Menue F3) - tools/boulder/k1_waechter.py nachziehen und gruen halten; jede Stelle mit "Boulder:" markiert. Kraepelin-Kopie-Abweichungen markieren (kopie_pruefen.py gruen). Kein Heap im Minutenpfad, keine Datei-Scans, kein DLS-Open im Minuten-Callback. Deutsche Bezeichner, Kommentare erklaeren das WARUM, Tests rufen echten Code (kein Nachbau), kein catch ohne Log. Nach jedem fertigen Teil committen (Trailer "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" und "Claude-Session: https://claude.ai/code/session_012RYr1ZRXBzb7Y9hLeMoBPc") und git push origin v2. main NICHT anfassen. Kannst du etwas nicht fertigstellen: committe den sauberen Teilstand und nenne es klar unter "offen" - das naechste Paket baut darauf auf.`

const PAKETE = [
  { key: 'AP1', text: `AP1 "Abweichungen der Kopie" (D1-D14 der Spezifikation): Kraepelins eigene Schlafsitzungen durch den Boulder-Schlafkern ersetzen, Gehfilter/Bewegung, Schritt-Nullung im Schlaf aus dem eigenen Schlaf, Upstream-Fehler beim Lesen der Minutenhistorie (V2), Puls dichter bei erkannter Aktivitaet (auch Velo-Kandidat). Pruefkorpus-Gleichheit fuer die SCHRITTE muss bleiben (Kriterium 2); wo sich Sitzungen aendern, Test anpassen und begruenden.` },
  { key: 'AP3', text: `AP3 "Zyklen, Live-Zustand, Nickerchen": Nacht an Pebbles Sitzungsliste als Zyklen (Sleep/RestfulSleep), offen bis zum Morgen, dann einzeln abgeschlossen, jeder Abschnitt genau einmal beim Telefon, keine 0-Minuten-Eintraege, nie loeschen/ersetzen von festen Sitzungen; Live-Zustand ueber ongoing-Sitzung + get_last_sleep_utc (Kriterium 4: im Schlaf >=95 % schlafend, auf den Fixture-Naechten messen und ehrlich berichten); Nickerchen ab etwa 45 min (V3). Echte activity_sessions.c in den Tests. Schlafsumme ohne Wachminuten ueber Zyklen (Kriterium 3).` },
  { key: 'AP5', text: `AP5 "Messung und Messstufe": Messstufe als einzige zentrale Einstellung (F3): Tabelle Stufe 1 = 10/10/aus/aus, Stufen 2-5 wie b25; eigenes HRM-Abo der Variante fuer Puls nachts/tags und HRV-Fenster; SpO2 ueber Pebbles interne Werte passend zur Stufe gesetzt; Menue auf der Uhr in settings/health.c ersetzt Pebbles Puls-/SpO2-Intervall durch die Messstufe; BlobDB-Whitelist fuer den Pref; HRV-Fix in hrm_manager.c (F2). Ein einziges dauerhaftes Puls-Abo, nur Intervall wechseln, im Rueckruf nie an-/abmelden. Tests mit echtem hrm_manager wo moeglich.` },
  { key: 'AP7', text: `AP7 "Strenge Weckregel" (AP7a+b): Haken in alarm.c: nie im Tiefschlaf, nur nach Bewegung, spaetestens zur Weckzeit, auch nach Neustart im Weckfenster; Tests mit echtem alarm.c und dem echten Live-Zustand der Variante (Kriterium 4).` },
  { key: 'AP4', text: `AP4 "Nacht ausgeben und speichern": Nachtsatz 0x4001 und HRV 0x4002 im b26-Format (Bruecken-App 0.50 liest sie; Pruefung: Boulders NachtParser/NachtHrvParser-Erwartungen bzw. die Referenzbytes), DLS-Anlage nie im Minuten-Callback (eigener Callback, einmal je Boot), Abschlusslauf bei "fertig" und bei jeder Aenderung bis "endgueltig", Persistenz klein, nur anhaengen, Neustart/Zeitsprung sicher, NachtBild fuer die spaetere Health-App. Pruefe auch, ob Boulder 0.50 Tag 84 in Pebbles Fassung 3 liest (~/Projects/Boulder, HealthDataParser) - nur feststellen und berichten.` },
  { key: 'AP8', text: `AP8 "Uebergang von b25/b26" (Kriterium 8, F1): b25/b26-Formate lesen (NachtBild-Datei, Zustand, Messstufe-Pref, liegende DLS-Saetze 0x4001/0x4002), nichts verloren, nichts doppelt; Typ-8-Sitzung im Speicher: einmaliger Neustart hingenommen (F1), letzte Nacht wird wiederhergestellt - per Test belegen.` },
  { key: 'AP6', text: `AP6 "Velo und Auto": Kandidat aus Minuten-/5-s-Merkmalen (Kadenz, Bewegung, Handlage, Puls), Mindestdauer; Nachfrage auf der Uhr ohne Telefon nach F4 (10-12 min und am Ende, spaetestens 45 min, ohne Antwort "Nein") mit dem in der Spezifikation gewaehlten Mechanismus; bestaetigt: Velo als Pebble "Open" (F5), Schritte streichen wie Pebbles Schlaf-Nullung, 0 aktive kcal (F7); "Driving?" auch in Tram/Zug (V5); Auto ohne Sitzung (V6); Mitternacht (V7); vor dem Ja (V8). Echte Fahrtdaten des Nutzers liegen noch NICHT vor: Schwellen als Startwerte setzen, mit gestellten Faellen testen und als "an echten Fahrten einstellen" offen lassen.` },
  { key: 'AP9', text: `AP9 "Diagnose und Messung" (AP9a+b): taeglicher Diagnosesatz (laengster Rechenschritt, kleinster freier Heap, Neustarts); QEMU qemu_emery: Rechenzeit je Minutenschritt und Abschlusslauf messen (Kriterium 7: < 100 ms reine Rechenzeit, Warten getrennt < 5 s), Speicher mit Ballast wie auf der Uhr, Nachfrage-Dialog sichtbar (Bild selbst ansehen). Die uebrigen Boards bauen (getafix, qemu_flint, qemu_gabbro: dort Original-Kraepelin) - offener Punkt aus AP0. Boulders Parser-Tests mit den Referenzdaten laufen lassen (~/Projects/Boulder, nur lesen/testen, nichts committen).` },
]

phase('Pakete')
const berichte = []
for (const p of PAKETE) {
  const vorher = berichte.map(b => `--- ${b.key} ---\n${b.bericht}`).join('\n')
  const r = await agent(`${GRUND}\n\nBISHERIGE PAKET-BERICHTE:\n${vorher || '(AP0, AP-G, AP2 fertig: Geruest, Referenzbytes b26, Schlafkern gleich b26 auf allen sechs Naechten; siehe git log)'}\n\nDEIN PAKET: ${p.text}`,
    { label: p.key, phase: 'Pakete' })
  berichte.push({ key: p.key, bericht: r || '(kein Bericht - Agent gescheitert)' })
  log(`${p.key} fertig`)
}

const ALLES = berichte.map(b => `--- ${b.key} ---\n${b.bericht}`).join('\n')

phase('Pruefung')
const pruefung = await agent(`${GRUND}\n\nPAKET-BERICHTE:\n${ALLES}\n\nDu bist Skeptiker. EINE Gegenpruefung fuer Schritt 1, nur die riskanten Teile: (1) Live-Zustand und Sitzungen in Pebbles Liste (Doppelte, Asserts, Neustart/Update in der Nacht, Zeitsprung), (2) Persistenz und Uebergang von b25/b26, (3) DLS und Watchdog/Rechenzeit/Speicher im Minutenpfad, (4) HRM-Abo und Messstufe (Use-after-free, Abo-Lecks), (5) Weckregel. Nur lesen, eigene Proben unter ${S}/v2s1/pruefung/. Liste nur echte Befunde (Ablauf, Datei:Zeile, Schwere, Fix), keine Stilfragen. Kurz.`,
  { label: 'gegenpruefung', phase: 'Pruefung' })

phase('Abnahme')
const abnahme = await agent(`${GRUND}\n\nPAKET-BERICHTE:\n${ALLES}\n\nGEGENPRUEFUNG:\n${pruefung}\n\nAUFGABE AP10: (1) Behebe alle berechtigten Befunde der Gegenpruefung (am Code nachpruefen) mit Test; (2) voller Testlauf und obelix-Release-Bau; (3) schreibe den Abnahmebericht nach ${V2}/schritt1-abnahme.md: je Abnahmekriterium 1-8 Stand (erfuellt / teilweise / erst im Geraetetest / offen) mit Beleg (Test, Messwert, Commit), und eine kurze Liste, was fuer den Geraetetest nach Schritt 3 bleibt. Gib als Antwort eine Zusammenfassung von hoechstens 20 Zeilen fuer den Nutzer (Deutsch, du-Form, ohne Fachwand).`,
  { label: 'AP10', phase: 'Abnahme' })

return { abnahme, pruefung, berichte }
