export const meta = {
  name: 'v2-schlaf-schritte-entwuerfe',
  description: 'V2: mehrere Entwuerfe fuer Schlaf- und Schrittsystem auf der Uhr (Garmin-Niveau, stabil, wenig Upstream-Eingriff), mit Recherche, Gutachten und Gegenueberstellung',
  phases: [
    { title: 'Recherche', detail: 'Firmware-Innenleben, Garmin und Fachliteratur' },
    { title: 'Entwuerfe', detail: 'vier unabhaengige Ansaetze' },
    { title: 'Gutachten', detail: 'Stabilitaet/Eingriff und Qualitaet/Nutzen' },
    { title: 'Gegenueberstellung', detail: 'fuer die Diskussion' },
  ],
}

const S = '/tmp/claude-1000/-home-dysseus-Documents-Claude/a0d87cde-04f6-43b4-bc17-a2150aed9501/scratchpad'
const LAGE = `LAGE (Boulder V2, Diskussionsphase, kein Code):
- Uhr: Pebble Time 2 (emery, Board obelix), Boulder-Firmware = Fork von PebbleOS (~/Projects/pebbleos, Doku BOULDER.md). Upstream: coredevices/PebbleOS (Remote "upstream", git fetch upstream; seit unserer Basis ~276 Commits, u. a. src/fw -> fw, neues Task-Watchdog-Subsystem subsys/task_wdt). Telefon-App Boulder (~/Projects/Boulder, Kotlin/Compose, Health Connect).
- V2 wird auf dem aktuellen Upstream neu aufgebaut, mit moeglichst kleinem, sauber gekapseltem Eingriff in die Firmware.
- ENTSCHEIDUNG DES NUTZERS: Schlaf und Schritte bleiben AUF DER UHR (nicht aufs Telefon). Dem Nutzer hat das bisherige System gefallen; es soll an unseren Fall angepasst und besser gestaltet werden. Zitat: "garmin kann es, dan geht das bei uns auch." Er erwartet mehrere Vorschlaege.
- HEUTIGER STAND (Boulder-Firmware b25): Pebbles Kraepelin zaehlt Schritte und erkennt Gehen/Laufen; Boulder verwirft Kraepelins Schlaf und rechnet eine eigene Nacht (src/fw/services/activity/boulder/nacht.c, schlafanalyse.c, hrv.c, messstufe.c): alle 15 min im Fenster 20-11 Uhr die ganze Nacht aus der Minutendatei neu (Cole-Kripke-artig + Puls/HRV-Regeln, Phasen Wach/Leicht/Tief/REM), ein Schlafblock je Nacht, Schlafsumme ohne Wachminuten, HRV-Fenster aus RR-Intervallen (RMSSD), Messstufe 1-5 fuer HR/SpO2/HRV-Takt, SpO2-Ring, Gehfilter (Velo ist kein Spaziergang, Typ 8 Movement), Datalogging an Boulder (Tags 0x4001/0x4002). Echte Naechte mit Fitbit-Vergleich als Fixtures in tests/fw/services/activity/ (26.09., 29.09., 30.09., 03.10.).
- WAS SCHIEF LIEF (Lehren): Die Uhr stuerzte am 04. und 05.10. morgens ab (05.10. genau 06:00 zur Weckzeit), Ursache ohne Coredump unbewiesen; Kandidaten: schwere Arbeit im Minuten-Callback auf KernelBG (20-KB-Heap je Rechnung, Flash-Vollscans, DLS-Open mit 4 s Wartezeit ohne Watchdog-Futter), voller DLS-Pool (Upstream-Assert). Viele Fehler entstanden an Uebergaengen zu Upstream-Code (activity_sessions.c, Kraepelin-Eingriffe, Doppelbloecke, Neustart am Morgen, Update b24->b25, Asserts beim Loeschen von Sitzungen). Der Gehfilter griff von b19 bis b23 nie (falscher Sitzungstyp), unbemerkt. Smart Alarm klingelte immer zu Fensterbeginn (Schlafzustand nachts "wach").
- Weitere Boulder-Funktionen, die mit Schlaf/Schritten zusammenhaengen: Ruhepuls (Median der Naechte), Herzgrenzen, Kieselsport-Training (eigene Sitzungen, 1-Hz-Puls), TimeStyle-Schlaf-/Schritt-Widgets (lesen health_service), Boulder-Health-App auf der Uhr (Karten Heute/Schlaf/Herz&SpO2).`

const FAKTEN_SCHEMA = { type: 'object', properties: { bericht: { type: 'string' } }, required: ['bericht'] }

phase('Recherche')
const [intern, extern] = await Promise.all([
  agent(`${LAGE}

AUFGABE: Faktenblatt "Innenleben" fuer die Entwerfer. Nur lesen (git fetch upstream ist erlaubt; nichts committen, keine Worktrees ausser unter ${S}/v2/). Untersuche im AKTUELLEN Upstream (upstream/main) und vergleiche mit unserem main:
1. Architektur des Aktivitaetsdienstes: activity.c, activity_algorithm.h (gibt es eine austauschbare Algorithmus-Schnittstelle? welche Funktionen, wer ruft sie wann, auf welchem Task), Kraepelin (Schrittzaehlung, Gehen/Laufen-Erkennung, Schlaf: wie, mit welchen Daten, wie gut, Speicher), Minutendaten-Pfad und Speicherdatei, Sitzungsliste (activity_sessions.c, Limits, Persistenz, Asserts), Metriken (activity_metrics.c), health_service-API fuer Apps, Insights, Smart Alarm (alarm.c), Workout-Dienst.
2. Task- und Ressourcenmodell auf obelix: Tasks (KernelMain, KernelBG, App, Worker, HRM ...), Prioritaeten, Stackgroessen, Kernel-Heap (Groesse, Fragmentierung), ob/wie man einen eigenen Kernel-Task oder eine eigene Arbeitsqueue anlegen kann, neues Task-Watchdog-Subsystem (Grenzen, Fuetterung), Flash/Settings-File-Kosten, CPU (Takt, typische Rechenzeiten), Datalogging-Pool (20 Plaetze) und DLS-Open-Verhalten.
3. Sensoren: Beschleunigungssensor (Rate, Puffer, Stromkosten), HRM GH3x2x (bpm, RR-Intervalle/HRV in Upstream? Qualitaet, Takt, Strom), SpO2.
4. Was unser Fork heute in diesen Bereichen aendert (BOULDER.md, Kconfig-Schalter, haken.h) und welche Stellen davon fehleranfaellig waren.
Liefere ein dichtes Faktenblatt (Deutsch) mit Datei:Zeile-Belegen fuer upstream/main, Zahlen wo moeglich, und einer Liste "saubere Einhaengepunkte" (wo ein eigener Algorithmus mit minimalem Eingriff andocken kann).`, { label: 'recherche:innen', phase: 'Recherche', schema: FAKTEN_SCHEMA }),
  agent(`${LAGE}

AUFGABE: Faktenblatt "Stand der Technik" fuer die Entwerfer. Nutze WebSearch/WebFetch (per ToolSearch laden). Untersuche:
1. Garmin: wie Schlaf erkannt und in Phasen eingeteilt wird (Firstbeat Analytics, Bewegung + HR + HRV, ggf. Atmung/SpO2), Sleep Score, Sleep Coach, nap detection, Body Battery/Stress (HRV-basiert), Move IQ / automatische Aktivitaetserkennung, Schrittzaehler-Genauigkeit und Fehlschritt-Unterdrueckung (z. B. Autofahren, Velo), was davon auf der Uhr und was in der Cloud/App laeuft.
2. Andere: Fitbit (Sleep Stages, Sleep Score), Apple Watch (Sleep Stages, Schritt/Aktivitaet), Oura, Whoop; veroeffentlichte Methoden.
3. Wissenschaft: Aktigraphie (Cole-Kripke, Sadeh, Oakley), Schlafphasen aus Handgelenk-Beschleunigung + PPG/HRV (z. B. Walch et al. 2019, Altini & Kinnunen 2021, Fonseca, de Zambotti), typische Genauigkeit (Uebereinstimmung je Minute, Kappa) fuer 4 Phasen, welche Merkmale tragen (HR-Trend, HRV RMSSD/LF/HF, Bewegungsaktivitaet, zirkadiane Merkmale, Atemrate aus HRV), kleine Modelle (Entscheidungsbaeume, logistische Regression, kleine HMMs) die auf einem Mikrocontroller mit wenig RAM laufen.
4. Schritte: Algorithmen (Spitzen/Autokorrelation/Frequenzanalyse), Unterdrueckung von Fehlschritten, Erkennung Gehen/Laufen/Velo/Fahren aus Beschleunigung + Puls + Kadenz, Genauigkeit.
5. Was ist mit Pebble-Hardware (emery: ARM Cortex-M33-Klasse SF32LB52 oder aehnlich, ~240 MHz?, Kernel-Heap ~97 KB, 3-Achsen-Beschleunigung, GH3x2x-PPG mit RR) realistisch?
Liefere ein dichtes Faktenblatt (Deutsch) mit Quellen (URLs), realistischen Zielwerten und konkreten, auf Pebble uebertragbaren Ideen.`, { label: 'recherche:aussen', phase: 'Recherche', schema: FAKTEN_SCHEMA }),
])

const FAKTEN = `FAKTENBLATT INNENLEBEN:\n${intern ? intern.bericht : '(fehlt)'}\n\nFAKTENBLATT STAND DER TECHNIK:\n${extern ? extern.bericht : '(fehlt)'}`

const ENTWURF_SCHEMA = { type: 'object', properties: {
  name: { type: 'string' }, kurz: { type: 'string' }, entwurf: { type: 'string' },
}, required: ['name', 'kurz', 'entwurf'] }

const ANSAETZE = [
  { key: 'plugin', text: `ANSATZ "Austauschbarer Algorithmus": Nutze (oder schaffe minimal) eine Algorithmus-Schnittstelle im Aktivitaetsdienst, so dass ein Boulder-Algorithmus Kraepelins Schlafteil sauber ersetzt bzw. ergaenzt, waehrend Schritte bei Kraepelin bleiben oder gezielt verbessert werden. Ziel: kleinster Upstream-Eingriff, klare Grenze, per Kconfig waehlbar.` },
  { key: 'strom', text: `ANSATZ "Inkrementell statt Neurechnung": Schlaf als Online-Algorithmus, der jede Minute einen kleinen Zustand fortschreibt (O(1) Speicher, keine Vollscans, kein 20-KB-Heap), mit kurzer Glaettung beim Aufwachen; live verfuegbarer Schlafzustand fuer Smart Alarm und Widgets; robust gegen Neustart (Zustand klein und persistent). Schritte und Aktivitaetstyp ebenfalls inkrementell.` },
  { key: 'garmin', text: `ANSATZ "Garmin-Niveau": Qualitaet zuerst. Mehrsensor-Modell (Bewegung, HR-Verlauf, HRV, ggf. Atemrate aus RR, SpO2, zirkadiane Merkmale, persoenliche Basiswerte) mit kleinem, auf dem Mikrocontroller laufenden Modell; Schlafphasen, Schlafscore, Nickerchen, Smart Alarm in leichtem Schlaf; Schritte mit Fehlschritt-Unterdrueckung und automatischer Aktivitaetserkennung (Gehen/Laufen/Velo/Fahren) aus Kadenz + Puls. Zeige, was davon realistisch ist und was es kostet.` },
  { key: 'isoliert', text: `ANSATZ "Eigener Gesundheitsdienst, isoliert": Boulders Schlaf/Schritt-Logik als eigener, abgeschotteter Dienst mit eigenem Task oder eigener Arbeitsqueue, festem Speicherbudget (statisch), eigener Persistenz und eigenem Datalogging-Kanal; nur lesende Andockpunkte an Upstream (Minutendaten, HRM-Abo), keine Eingriffe in Sitzungsliste/Kraepelin; Ergebnisse fuer Apps ueber eine eigene kleine API bzw. ueber health_service-Haken. Fokus: Stabilitaet (nie wieder ein Absturz durch Boulder-Code) und Upstream-Merges ohne Konflikte.` },
]

phase('Entwuerfe')
const entwuerfe = (await parallel(ANSAETZE.map(a => () => agent(`${LAGE}

${FAKTEN}

${a.text}

AUFGABE: Arbeite einen eigenstaendigen, konkreten Vorschlag fuer das Schlaf- und Schrittsystem AUF DER UHR fuer Boulder V2 aus (nur Entwurf, kein Code, nichts in ~/Projects aendern; Lesen und kleine Messungen/Proben unter ${S}/v2/${a.key}/ erlaubt). Gliederung:
1. Kernidee in 3 Saetzen.
2. Aufbau: Bausteine, wo sie in der Firmware liegen, welche Upstream-Stellen beruehrt werden (Datei/Funktion, Art des Eingriffs), Task/Takt, Speicherbudget (Zahlen), Persistenz, Datenweg zum Telefon (Vertrag), was Apps (TimeStyle, Health-App, Smart Alarm) sehen.
3. Schlaf: Erkennung, Phasen, ein Block je Nacht, Nickerchen, Smart Alarm, Neustart/Update in der Nacht, ohne Telefon.
4. Schritte und Aktivitaeten: was bleibt von Pebble, was wird besser (z. B. Velo/Fahren), wie.
5. Was wird besser als heute (konkret, messbar), was bleibt wie heute, was wird schlechter.
6. Stabilitaet: wie die bekannten Absturzquellen ausgeschlossen sind.
7. Pruefbarkeit: Host-Tests mit echten Naechten, Fitbit-Vergleich, Abnahmekriterien.
8. Aufwand (grobe Groessenordnung), Risiken, offene Fragen an den Nutzer.
Deutsch, konkret, mit Zahlen und Belegen aus den Faktenblaettern. Keine Textwand im Kern: Kernidee und Abschnitt 5 kurz und klar.`,
  { label: `entwurf:${a.key}`, phase: 'Entwuerfe', schema: ENTWURF_SCHEMA }).then(r => r ? { key: a.key, ...r } : null)))).filter(Boolean)

const ENTWUERFE_TEXT = entwuerfe.map(e => `### Entwurf ${e.key}: ${e.name}\n${e.kurz}\n\n${e.entwurf}`).join('\n\n---\n\n')

phase('Gutachten')
const [stabil, qualitaet] = await Promise.all([
  agent(`${LAGE}\n\n${FAKTEN}\n\nENTWUERFE:\n${ENTWUERFE_TEXT}\n\nDu bist Gutachter fuer STABILITAET und UPSTREAM-EINGRIFF. Pruefe jeden Entwurf am echten Code (~/Projects/pebbleos, upstream/main; nur lesen): Sind die genannten Einhaengepunkte real? Sind Speicher-/Zeitbudgets plausibel? Welche Absturz- und Merge-Risiken bleiben? Was ist uebersehen? Bewerte je Entwurf (1-5) Stabilitaet, Upstream-Eingriff (5 = minimal), Aufwand (5 = klein), mit Begruendung, und nenne Verbesserungen, die einen Entwurf deutlich staerker machen. Deutsch, knapp.`, { label: 'gutachten:stabilitaet', phase: 'Gutachten' }),
  agent(`${LAGE}\n\n${FAKTEN}\n\nENTWUERFE:\n${ENTWUERFE_TEXT}\n\nDu bist Gutachter fuer QUALITAET und NUTZEN aus Sicht des Nutzers (er mochte das bisherige System, will Garmin-Niveau). Pruefe: Wie gut werden Schlafphasen, Einschlafen/Aufwachen, Nickerchen, Smart Alarm, Schritte, Aktivitaetserkennung realistisch (Zahlen aus der Literatur)? Was merkt der Nutzer im Alltag? Was fehlt gegenueber Garmin, was ist auf Pebble-Hardware nicht erreichbar? Bewerte je Entwurf (1-5) Schlafqualitaet, Schrittqualitaet, Alltagsnutzen, mit Begruendung, und nenne Verbesserungen. Deutsch, knapp.`, { label: 'gutachten:qualitaet', phase: 'Gutachten' }),
])

phase('Gegenueberstellung')
const synthese = await agent(`${LAGE}\n\nENTWUERFE:\n${ENTWUERFE_TEXT}\n\nGUTACHTEN STABILITAET/EINGRIFF:\n${stabil}\n\nGUTACHTEN QUALITAET/NUTZEN:\n${qualitaet}\n\nAUFGABE: Schreibe die Gegenueberstellung fuer die Diskussion mit dem Nutzer (Deutsch, klar, keine Textwand, du-Form). Uebernimm die Gutachter-Verbesserungen in die Entwuerfe, wo sie passen; zwei Entwuerfe duerfen verschmolzen werden, wenn sie sich kaum unterscheiden; es sollen am Ende 3 bis 4 klar unterscheidbare Vorschlaege bleiben. Je Vorschlag: Name, Kernidee (2-3 Saetze), was besser wird als heute (konkret), Preis/Risiko, Aufwand, Stabilitaet. Dann eine Vergleichstabelle (Schlafqualitaet, Schritte/Aktivitaeten, Stabilitaet, Upstream-Eingriff, Aufwand). Dann eine Empfehlung mit Begruendung und die 3-5 wichtigsten Fragen, die der Nutzer entscheiden muss. Zahlen nur, wenn sie aus Entwuerfen/Gutachten belegt sind; Schaetzungen als Schaetzung kennzeichnen.`, { label: 'synthese', phase: 'Gegenueberstellung' })

return { synthese, entwuerfe, gutachten: { stabil, qualitaet }, fakten: { intern: intern && intern.bericht, extern: extern && extern.bericht } }
