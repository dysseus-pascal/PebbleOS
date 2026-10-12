export const meta = {
  name: 'v2-ap0-geruest',
  description: 'V2 Schritt 1, Arbeitspaket AP0 + AP-G (schlank, ein Agent): Kraepelin-Variante als Geruest auf Branch v2, Referenzbytes der Testnaechte aus b26',
  phases: [{ title: 'AP0', detail: 'ein Agent' }],
}
const S = '/tmp/claude-1000/-home-dysseus-Documents-Claude/a0d87cde-04f6-43b4-bc17-a2150aed9501/scratchpad'
const V2 = '/home/dysseus/Documents/Claude/boulder-v2'
phase('AP0')
const r = await agent(`Boulder V2, Schritt 1 "eigenes Kraepelin", Arbeitspakete AP0 (Geruest) und AP-G (Referenzbytes). SCHLANK arbeiten (Nutzerwunsch wegen Token-Verbrauch): keine Nebenrecherchen, keine langen Dokumente, Tests gezielt, Bericht hoechstens 25 Zeilen.

LIES ZUERST: ${V2}/entscheidungen.md (verbindlich, inkl. der Entscheidungen zur Spezifikation vom 06.10.) und in ${V2}/schritt1-spezifikation.md die Abschnitte zu Architektur, AP0 und AP-G. Die Spezifikation entstand vor den Entscheidungen F1-F9; wo sie abweicht, gelten die Entscheidungen (z. B. F3: Messstufe als einzige Einstellung, Menue settings/health.c; F6/F9: Regeln aus b26).

ORT: Worktree ${S}/v2s1/wt-basis auf Branch v2 (= upstream/main v4.39.0; falls der Worktree fehlt: git -C ~/Projects/pebbleos worktree prune und neu anlegen: git -C ~/Projects/pebbleos worktree add ${S}/v2s1/wt-basis v2; Submodule mit 'git submodule update --init' und --reference auf ~/Projects/pebbleos/<pfad>). Der pbl-Befehl muss aus dem Worktree kommen: PYTHONPATH=<wt>/tools/libs/pbl-cli. Umgebung: . ~/pebbleos-sdk-0.1.8/env.sh && . ~/Projects/pebbleos/.venv/bin/activate.
Nach jedem fertigen Teil committen (Trailer: "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" und "Claude-Session: https://claude.ai/code/session_012RYr1ZRXBzb7Y9hLeMoBPc") und den Branch v2 nach origin pushen (git push origin v2) - /tmp ist fluechtig. main nicht anfassen.

AP0:
- Kconfig-Schalter fuer die Algorithmus-Wahl (Kraepelin oder Boulder-Kraepelin), Vorgabe Boulder nur fuer obelix und qemu_emery; CMake waehlt die Quellen. Eingriffe in Pebble-Dateien mit "Boulder:" markieren.
- Kopie von Kraepelin (Glue + Kern) in einen eigenen Ordner, Herkunft (Upstream-Commit) im Kopf vermerkt, zunaechst inhaltlich unveraendert ausser Umbenennung, damit beide gleichzeitig gebaut werden koennen falls noetig. Pruefskript, das meldet, wenn sich das Original upstream aendert.
- Vertragstests/Waechter fuer die geteilten Formate (Tag 84, Sitzungstypen, DLS 0x4001/0x4002 Satzlayout wie in main/b26), uebernommen aus main wo sinnvoll.
- Nachweis: Die Variante (noch unveraendert) zaehlt auf Pebbles Pruefkorpus (test_kraepelin_algorithm o. ae.) exakt wie das Original. Baut fuer obelix@pvt Release, qemu_emery und mindestens ein anderes Board (dort Original). Host-Tests: Baseline vs. jetzt, keine neuen Fehler.
AP-G:
- Referenzbytes: Fuer jede Fixture-Nacht in main (b26; ~/Projects/pebbleos main, tests/fixtures/activity/boulder/ bzw. tests/fw/services/activity/) die Ausgabe der b26-Schlafanalyse (Nachtsatz 0x4001 als Bytes und Phasen je Minute) erzeugen und als Testvektoren in v2 ablegen, damit der V2-Schlafkern spaeter gegen b26 gleich gerechnet werden kann. Nur Testdaten, keine Produktionslogik.
BERICHT (kurz): was gebaut, Commits, Testergebnis, Abweichungen von der Spezifikation, offene Punkte.`, { label: 'ap0', phase: 'AP0' })
return r
