export const meta = {
  name: 'b26-einschlafen-regeln',
  description: 'b26: strengerer Schlafbeginn mit Pulsbedingung (F6) und Regeln an Fitbit-Naechten nachstellen (F9), nur in schlafanalyse.c, mit Kreuzvalidierung, Tests und Gegenpruefung',
  phases: [
    { title: 'Auswerten', detail: 'b25 auf allen Fitbit-Naechten, Regelkandidaten mit Kreuzvalidierung' },
    { title: 'Einbauen', detail: 'Worktree auf main, Tests, Mutation, Bau' },
    { title: 'Gegenpruefen', detail: 'Ueberanpassung, Stabilitaet, Testguete' },
  ],
}

const S = '/tmp/claude-1000/-home-dysseus-Documents-Claude/a0d87cde-04f6-43b4-bc17-a2150aed9501/scratchpad'
const V2 = '/home/dysseus/Documents/Claude/boulder-v2'
const LAGE = `KONTEXT: Boulder-Firmware V1 (~/Projects/pebbleos, Branch main = b25, laeuft beim Nutzer seit Installation ohne Absturz). Ziel b26: NUR die reine Schlafrechnung in src/fw/services/activity/boulder/schlafanalyse.c (+ Header/Tests) aendern. nacht.c, hrv.c, activity_sessions.c, Datalogging, Persistenz, Laufzeitverhalten NICHT anfassen (Stabilitaet von b25 ist heilig). Das Satzformat 0x4001/0x4002 bleibt Byte fuer Byte (Boulder 0.50 muss es lesen).
NUTZERENTSCHEIDUNGEN:
- F6: Schlafbeginn strenger: z. B. mindestens 15 ruhige Minuten UND eine Pulsbedingung (Puls deutlich unter dem Abendniveau bzw. nahe am Nachtniveau). Nur uebernehmen, wenn der Live-Schlafzustand nicht leidet (Test schlafzustand_ist_nachts_schlafend bzw. >=95 % der Schlafminuten schlafend).
- F9: Verfehlen die b25-Regeln eine Fitbit-Nacht beim Einschlafen oder Aufwachen um mehr als +-10 min, werden die Regeln nachgestellt - mit getrennten Einstell- und Pruefnaechten (Kreuzvalidierung: an 3 Naechten einstellen, an der 4. pruefen, reihum).
REFERENZNAECHTE (Fitbit): 29.09., 30.09., 03.10. (Fixtures und Fitbit-Tabellen in tests/fw/services/activity/, test_boulder_schlaf.c) und NEU 05.10. (Datei ${V2}/naechte/nacht-2026-10-05.csv = Boulder-Nachtexport mit Minutendaten und HRV-Fenstern; Fitbit-Bild ${V2}/naechte/fitbit-2026-10-05.png). Fuer den 05.10. gilt: Einschlafen Fitbit 22:15; Aufwachen 06:00 (vom Nutzer bestaetigt: Wecker, Boulder lag richtig, Fitbits 05:42 ist falsch). Fitbit-Phasen 05.10. nur von Hand aus dem Bild abgelesen (ungefaehr): REM 22:52-23:12, 00:28-00:49, 02:08-02:54, 03:56-04:30, 05:16-05:42; Tief 22:15-22:29, 23:53-00:09, 01:14-01:22, 03:22-03:41; sonst Leicht; Fitbits Summen: REM 134, Leicht 269, Tief 44, Wach 0 (32 min unruhig). b25 lieferte: Einschlafen 21:46, Aufwachen 06:00, Schlaf 479 min, Puls 21:46-22:15 im Mittel 69, 01-04 Uhr 59.
BISHERIGE UNTERGRENZEN (duerfen nicht sinken): Uebereinstimmung je Minute 29.09 >= 559 Promille, 30.09 >= 463, 03.10 >= 580 (siehe Tests).
UMGEBUNG: . ~/pebbleos-sdk-0.1.8/env.sh && . ~/Projects/pebbleos/.venv/bin/activate. Host-Tests: cmake -G Ninja -S tests -B <dir>; cmake --build <dir> --target <ziel>; ctest. Arbeitsdateien unter ${S}/b26/ (fluechtig). Ergebnisberichte zusaetzlich nach ${V2}/b26-*.md. Deutsch. Code-Regeln: deutsche Bezeichner, Kommentare erklaeren das WARUM, Tests kopieren keinen Produktionscode, Mutation je Fix, kein catch ohne Log. Commits mit Trailer:
Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_012RYr1ZRXBzb7Y9hLeMoBPc`

phase('Auswerten')
const auswertung = await agent(`${LAGE}

AUFGABE "Auswerten" (nur lesen und rechnen, nichts committen; Proben in ${S}/b26/probe/):
1. Baue einen Host-Lauf (oder Python-Nachbau, der die ECHTE schlafanalyse.c ueber ein kleines C-Harness aufruft - keinen Nachbau der Regeln), der fuer jede der 4 Fitbit-Naechte Einschlafen, Aufwachen, Schlafminuten, Phasensummen und Uebereinstimmung je Minute mit Fitbit liefert. Fuer den 05.10. die CSV in das Eingabeformat der Analyse bringen (Minutensaetze inkl. Puls, VMC, Lage; HRV-Fenster), so wie die anderen Fixtures es tun.
2. Halte die Abweichungen von b25 je Nacht fest (Einschlafen/Aufwachen in Minuten).
3. Entwickle 2-4 Regelkandidaten fuer F6 (strengerer Beginn mit Pulsbedingung; z. B. Puls unter Abend-Median minus X oder innerhalb Y von einem Nachtniveau, das robust aus der Nacht selbst kommt; ruhige Minuten >= 15) und ggf. F9-Anpassungen am Aufwachen. Bewerte jeden Kandidaten mit Kreuzvalidierung (Parameter nur an 3 Naechten waehlen, an der 4. messen, reihum) - berichte die Pruef-Ergebnisse, nicht nur die Einstell-Ergebnisse.
4. Pruefe fuer jeden Kandidaten: Uebereinstimmung je Minute auf 29.09/30.09/03.10 nicht unter den Untergrenzen; Live-Zustand (Test schlafzustand_ist_nachts_schlafend in test_boulder_nacht.c) bleibt; keine Laufzeit-Aenderung noetig.
5. Empfiehl EINEN Kandidaten mit Begruendung, oder sage klar, dass keiner ueber alle Naechte traegt (dann das ehrlich so). Schreibe den Bericht nach ${V2}/b26-auswertung.md und gib ihn zurueck.`, { label: 'auswerten', phase: 'Auswerten' })

phase('Einbauen')
const einbau = await agent(`${LAGE}

AUSWERTUNG:\n${auswertung}

AUFGABE "Einbauen": Arbeite in einem eigenen Worktree: git -C ~/Projects/pebbleos worktree add ${S}/b26/wt -b b26-einschlafen main (Submodule fuer Builds per Symlink aus ~/Projects/pebbleos einhaengen, am Ende aushaengen; Worktree stehen lassen). Committe nach jedem fertigen Teilschritt.
1. Nimm die Nacht 05.10. als Fixture auf (Format wie die anderen), mit Fitbit-Referenz (Einschlafen 22:15, Aufwachen 06:00, Phasen als ungefaehre Tabelle mit Kommentar "von Hand aus dem Bild abgelesen").
2. Setze den empfohlenen Kandidaten aus der Auswertung in schlafanalyse.c um (nur dort plus Header). Falls die Auswertung sagt, dass kein Kandidat traegt: NICHT einbauen, sondern das berichten.
3. Tests: je Fitbit-Nacht Einschlafen und Aufwachen innerhalb +-10 min (oder, wenn eine Nacht das nachweislich nicht schafft, die tatsaechliche Abweichung als ehrliche Grenze mit Kommentar), Uebereinstimmungs-Untergrenzen unveraendert oder hoeher, neue Untergrenze fuer 05.10. (ehrlich, mit Kommentar zur Unschaerfe der Bild-Referenz), Live-Zustand-Test gruen. Mutation: Regel zuruecknehmen -> Einschlaf-Test 05.10. rot.
4. Alle Host-Tests (bekannte 11 Upstream-Fehler ausgenommen), obelix-Release-Bau (bundle.sh-Weg nicht noetig, pbl build reicht).
Bericht (Deutsch): was geaendert, Zahlen vorher/nachher je Nacht, Tests, Mutation, Commit, Worktree-Pfad. Auch nach ${V2}/b26-einbau.md.`, { label: 'einbauen', phase: 'Einbauen' })

phase('Gegenpruefen')
const gegen = await agent(`${LAGE}

AUSWERTUNG:\n${auswertung}\n\nEINBAU-BERICHT:\n${einbau}

Du bist Skeptiker. Pruefe den Branch b26-einschlafen (Worktree ${S}/b26/wt, nur lesen; eigene Proben unter ${S}/b26/gegen/): (1) Ist es Ueberanpassung an den 05.10.? Stimmen die Kreuzvalidierungs-Zahlen beim Nachrechnen? (2) Wurde wirklich nur die reine Rechnung geaendert (git diff main..b26-einschlafen), Satzformat und Laufzeit unberuehrt? (3) Pruefen die Tests das Behauptete, ohne Produktionscode zu kopieren; greift die Mutation? (4) Schaden fuer Faelle ohne Puls (Messstufe 1, Puls aus), Nickerchen-artige Naechte, sehr spaetes Zubettgehen, Nachtschicht? Stelle mindestens zwei solche Faelle nach. Urteil: freigeben / nachbessern (mit konkreten Punkten). Deutsch, knapp. Auch nach ${V2}/b26-gegenpruefung.md.`, { label: 'gegenpruefen', phase: 'Gegenpruefen' })

return { auswertung, einbau, gegen }
