export const meta = {
  name: 'v2-schritt1-spezifikation',
  description: 'V2 Schritt 1 (eigenes Kraepelin): Branch v2 auf upstream/main aufsetzen, b25-Bestand inventarisieren, technische Spezifikation schreiben, zweifach begutachten, ueberarbeiten',
  phases: [
    { title: 'Grundlage', detail: 'Branch v2, Bauprobe, Inventar b25' },
    { title: 'Spezifikation', detail: 'Entwurf' },
    { title: 'Gutachten', detail: 'Stabilitaet/Eingriff, Kriterien/Pruefbarkeit' },
    { title: 'Ueberarbeitung', detail: 'Endfassung + offene Produktfragen' },
  ],
}

const S = '/tmp/claude-1000/-home-dysseus-Documents-Claude/a0d87cde-04f6-43b4-bc17-a2150aed9501/scratchpad'
const V2 = '/home/dysseus/Documents/Claude/boulder-v2'
const LAGE = `KONTEXT Boulder V2, Schritt 1 "eigenes Kraepelin".
- Entscheidungen und die vom Nutzer bestaetigten Abnahmekriterien: ${V2}/entscheidungen.md (LESEN, verbindlich). Ideenliste: ${V2}/ideen.md (nicht umsetzen).
- Hintergrund: Vorschlaege und Gutachten in ${V2}/ (schlaf-schritte-gegenueberstellung.md, entwurf-*.md, gutachten-*.md, fakten-innen.md mit Datei:Zeile fuer upstream/main, fakten-aussen.md).
- Firmware-Repo ~/Projects/pebbleos: main = Bruecke b25 (Boulder-Fork, Referenzcode fuer Schlafanalyse, HRV, Messstufe, SpO2-Ring, Gehfilter, Nacht-DLS 0x4001/0x4002, Tests und echte Fixture-Naechte unter tests/fw/services/activity/). Remote upstream = coredevices/PebbleOS; V2 baut auf upstream/main (v4.39.0).
- Telefon: Bruecken-App Boulder 0.50 (~/Projects/Boulder) empfaengt Naechte und HRV auf 0x4001/0x4002 (BoulderDls.kt, NachtParser.kt, NachtHrvParser.kt) - das Satzformat muss so bleiben, dass 0.50 es versteht.
- Leitlinie: Der Nutzer will wenig Eingriff in Pebbles Code und eine Uhr, die ohne Telefon voll funktioniert. Nur die in Kriterium 1 genannten Pebble-Stellen duerfen beruehrt werden (Algorithmus-Wahl in Kconfig/CMake, Messstufe-Whitelist, strenge Weckregel in alarm.c). Braucht etwas zwingend einen weiteren Eingriff, ist das eine offene Frage an den Nutzer, kein stiller Zusatz.
- Lehren aus b20-b25: keine schwere Arbeit im Minuten-Callback (kein Heap, keine Datei-Scans, kein DLS-Open dort), keine laufenden Sitzungen ersetzen/loeschen, Filter muessen per Test nachweislich greifen, Zeitspruenge/Neustart/Update beachten.
- Umgebung Firmware: . ~/pebbleos-sdk-0.1.8/env.sh && . ~/Projects/pebbleos/.venv/bin/activate (Submodule fuer Builds in Worktrees per Symlink aus ~/Projects/pebbleos einhaengen und am Ende aushaengen). Host-Tests: cmake -G Ninja -S tests -B <dir>; cmake --build <dir> -- -k 0; ctest --test-dir <dir> --timeout 300.
- Ablage: Arbeitsdateien unter ${S}/v2s1/ (fluechtig, /tmp ist tmpfs); Ergebnisdokumente IMMER zusaetzlich nach ${V2}/ schreiben. Deutsch.`

phase('Grundlage')
const [basis, inventar] = await Promise.all([
  agent(`${LAGE}

AUFGABE "Grundlage":
1. Lege in ~/Projects/pebbleos den lokalen Branch v2 auf upstream/main an (git fetch upstream; git branch v2 upstream/main). NICHT pushen, main nicht anfassen. Arbeite in einem Worktree unter ${S}/v2s1/wt-basis (git worktree add ... v2), Submodule passend zu upstream/main initialisieren (git -C <wt> submodule update --init, notfalls mit --reference auf ~/Projects/pebbleos/<pfad>); falls das wegen Netz/Rechten scheitert, beschreibe es genau.
2. Baue upstream/main fuer obelix@pvt (Release) und qemu_emery und fuehre die Host-Tests aus. Halte fest: baut es, welche Tests scheitern schon in upstream (Baseline), Speicherbelegung (text/data/bss, Kernel-Heap), Bauzeit.
3. Pruefe in der Bruecken-App ~/Projects/Boulder, wie eine per Datei geoeffnete .pbz installiert wird (Rueckfrage, wird die Ed25519-Signatur geprueft oder nicht?) - wichtig fuer Vorabversionen.
4. Pruefe, ob upstream/main inzwischen etwas enthaelt, das unseren Plan beruehrt (z. B. Aenderungen an activity_algorithm.h, Kraepelin, activity_sessions.c, alarm.c, hrm_manager, Datalogging-Tags, Health-App, workout_service), mit Commit-IDs.
Liefere einen knappen Bericht (Deutsch) und schreibe ihn auch nach ${V2}/schritt1-grundlage.md. Den Worktree am Ende stehen lassen (wird weiterverwendet), Submodul-Symlinks aushaengen.`, { label: 'grundlage:branch', phase: 'Grundlage' }),
  agent(`${LAGE}

AUFGABE "Inventar b25": Gehe den Boulder-Code auf ~/Projects/pebbleos main durch, der fuer Schritt 1 relevant ist: services/activity/boulder/ (nacht.c, schlafanalyse.c, hrv.c, messstufe.c, haken.h usw.), SpO2-Ring, Gehfilter/Movement-Eingriffe in Kraepelin, Eingriffe in activity.c/activity_sessions.c/activity_metrics.c/alarm.c/prefs/settings_blob_db.c, DLS-Vertrag 0x4001/0x4002 (Satzlayout, Fassungen), vertraege.c/.py, Tests und Fixtures (welche echten Naechte, Fitbit-Vergleichsdaten, Toleranzen). Fuer jedes Teil: Zweck, Groesse, ob es in die Kraepelin-Variante uebernommen werden kann (unveraendert / angepasst / entfaellt), und warum. Besonders: Laesst sich die Schlafanalyse (schlafanalyse.c) unveraendert als Rechenkern weiterverwenden (reine Funktion? Eingaben/Ausgaben?), und welche Teile von nacht.c waren nur noetig, weil Kraepelin daneben lief? Welche Tests/Fixtures sind direkt fuer die Abnahmekriterien 2-8 wiederverwendbar?
Liefere einen dichten Bericht (Deutsch, Datei:Zeile) und schreibe ihn nach ${V2}/schritt1-inventar.md. Nur lesen.`, { label: 'grundlage:inventar', phase: 'Grundlage' }),
])

phase('Spezifikation')
const entwurf = await agent(`${LAGE}

GRUNDLAGE-BERICHT:\n${basis}\n\nINVENTAR b25:\n${inventar}

AUFGABE: Schreibe die technische Spezifikation fuer Schritt 1 nach ${V2}/schritt1-spezifikation.md (Deutsch, klar gegliedert, keine Textwand; Tabellen wo sinnvoll). Inhalt:
1. Ueberblick und Abgrenzung (was Schritt 1 umfasst, was erst in Schritt 2/3 bzw. im App-Schritt kommt, z. B. GPS-Bestaetigung, Coredump-Abholung, Kieselsport, Health-App).
2. Architektur der Variante: Ordner, Kconfig-Schalter, CMake-Auswahl, welche Kraepelin-Dateien kopiert und wie markiert (Herkunft upstream-Commit), Pflege der Kopie (Pruefskript gegen das Original), alle 21 Funktionen von activity_algorithm.h und wie die Variante sie erfuellt.
3. Schritte: unveraenderte Zaehlung, Nachweis der Gleichheit auf Pebbles Pruefkorpus; Velo/Auto: Erkennung der Kandidatin (Merkmale, Schwellen als Startwerte, Mindestdauer), Nachfrage auf der Uhr ohne Telefon (WIE genau: welcher Mechanismus zeigt eine Frage mit Ja/Nein auf der Uhr an, ohne weiteren Pebble-Eingriff? Wenn das nicht ohne Eingriff geht: Optionen mit Aufwand als offene Frage), Loggen als Velo, Schritte nicht zaehlen, Puls dichter bei erkannter Aktivitaet (wie Kraepelin fuer Gehen/Laufen, plus Velo).
4. Schlaf: Live-Erkennung mit Puls im Kraepelin-Takt, Sitzungen als Zyklen (Sleep/RestfulSleep, Pebble-Semantik, ongoing-Meldung, get_last_sleep_utc fuer den Live-Zustand), Nickerchen, Phasen nach dem Nachlauf-Prinzip mit den b25-Regeln (Gleichheit 0 abweichende Minuten auf den Fixtures als Pflicht), Nachtsatz 0x4001 im b25-Format fuer die Bruecken-App, HRV-Fenster 0x4002, SpO2-Ring.
5. Messstufe: eigene Einstellung (1 Byte, Whitelist), Tabelle (Stufe 1 = Pebble-Original 10/10/aus/aus, Stufen 2-5 wie b25), wie Puls nachts/tags, HRV und SpO2 gesteuert werden OHNE weitere Eingriffe in activity.c (z. B. eigenes HRM-Abo der Variante wie Kraepelins Aktivitaets-Puls; SpO2-Takt?) - wenn ein weiterer Eingriff unvermeidbar ist: offene Frage mit Begruendung.
6. Smart Alarm streng: genaue Regel, Haken in alarm.c (Zeilen), was die Variante dafuer liefert.
7. Stabilitaet und Ressourcen: Arbeit pro Minute (Budget in us/ms, Messplan in QEMU), Speicher (statisch, kein Heap im Minutenpfad), Persistenz (was, wo, wie oft, Neustart/Zeitsprung), DLS-Anlage ausserhalb des Minuten-Callbacks, Datalogging-Pool.
8. Uebergang von b25 (Kriterium 8): Prefs, Sitzungen, letzte Nacht, liegende DLS-Saetze - nichts verloren, nichts doppelt.
9. Testplan je Abnahmekriterium (Host-Tests, Fixtures, Mutationen, QEMU-Messungen), und was erst im Geraetetest nach Schritt 3 abgenommen wird.
10. Arbeitspakete fuer die Umsetzung mit Abhaengigkeiten und Reihenfolge (so geschnitten, dass mehrere Agenten in getrennten Worktrees parallel arbeiten koennen, wo moeglich), je Paket Ergebnis und Tests.
11. Offene Produktfragen an den Nutzer (nur echte Verhaltensentscheidungen), je mit Optionen und Empfehlung. Technische Wahlen entscheidest du selbst und begruendest sie.
Belege Aussagen ueber Pebble-Code mit Datei:Zeile (upstream/main). Gib als Antwort die Spezifikation zurueck.`, { label: 'spezifikation:entwurf', phase: 'Spezifikation' })

phase('Gutachten')
const [g1, g2] = await Promise.all([
  agent(`${LAGE}\n\nSPEZIFIKATION (Entwurf):\n${entwurf}\n\nDu bist Gutachter fuer STABILITAET und UPSTREAM-EINGRIFF. Pruefe am Code (upstream/main in ~/Projects/pebbleos, nur lesen; der Worktree ${S}/v2s1/wt-basis darf gelesen werden): Stimmen die Einhaengepunkte und Annahmen ueber Tasks, Sperren, Speicher, HRM-Abo aus dem Algorithmus, Sitzungs-Semantik, Persistenz, DLS? Bleibt der Eingriff wirklich auf Kriterium 1 begrenzt? Welche Absturz-, Daten- oder Merge-Risiken bleiben? Was fehlt? Liste konkrete Maengel mit Schwere und Verbesserung. Deutsch, knapp.`, { label: 'gutachten:stabilitaet', phase: 'Gutachten' }),
  agent(`${LAGE}\n\nSPEZIFIKATION (Entwurf):\n${entwurf}\n\nDu bist Gutachter fuer ABNAHMEKRITERIEN und PRUEFBARKEIT. Gehe die 8 Kriterien aus ${V2}/entscheidungen.md einzeln durch: Deckt die Spezifikation jedes vollstaendig ab? Ist jeder Test geeignet, das Kriterium zu belegen (nicht nur gestellt, kein kopierter Produktionscode, Mutation moeglich)? Sind die Daten dafuer vorhanden (Fixtures, Pruefkorpus, Kieselsport-Velofahrten)? Was wird nur behauptet? Sind die Arbeitspakete sinnvoll geschnitten? Sind die offenen Produktfragen echte Nutzerentscheidungen? Liste konkrete Maengel mit Schwere und Verbesserung. Deutsch, knapp.`, { label: 'gutachten:kriterien', phase: 'Gutachten' }),
])

phase('Ueberarbeitung')
const ende = await agent(`${LAGE}\n\nSPEZIFIKATION (Entwurf):\n${entwurf}\n\nGUTACHTEN STABILITAET:\n${g1}\n\nGUTACHTEN KRITERIEN:\n${g2}\n\nAUFGABE: Ueberarbeite die Spezifikation: Uebernimm berechtigte Maengel (am Code nachpruefen), weise unberechtigte mit kurzer Begruendung zurueck (Anhang "Nicht uebernommen"). Schreibe die Endfassung nach ${V2}/schritt1-spezifikation.md (ueberschreiben). Gib danach als Antwort NUR zurueck: (a) eine Zusammenfassung in 8-12 Zeilen fuer den Nutzer (Deutsch, du-Form, keine Fachwand), (b) die offenen Produktfragen mit Optionen und Empfehlung, (c) die Liste der Arbeitspakete mit Reihenfolge.`, { label: 'ueberarbeitung', phase: 'Ueberarbeitung' })

return { ende, basis, inventar }
