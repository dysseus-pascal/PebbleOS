export const meta = {
  name: 'boulder-screens-und-korrelation',
  description: 'Alle Boulder-Bildschirme rendern und auf Design-Konstanz prüfen; Konzept "Correlation" (eigene Karten mit zwei Werten) statt Trends ausarbeiten, mit Entwürfen und Aufwandschätzung',
  phases: [
    { title: 'Screens', detail: 'jeden Bildschirm mit Beispieldaten rendern' },
    { title: 'Design-Prüfung', detail: 'Konstanz gegen Baukasten, Farben, Formate' },
    { title: 'Correlation', detail: 'Konzept, Datenlage, Entwürfe' },
    { title: 'Aufwand', detail: 'Schätzung und Gegenprüfung' },
  ],
}
const V1 = '/home/dysseus/Documents/Claude/boulder-v1'
const S = '/tmp/claude-1000/-home-dysseus-Documents-Claude/a0d87cde-04f6-43b4-bc17-a2150aed9501/scratchpad/screens'
const GRUND = `Boulder (Android-App, ~/Projects/Boulder, main 34fc13cb = 0.52.0). NICHTS auf main committen oder pushen: arbeite in einem eigenen Worktree ${S}/wt (git worktree add ... -b screens-wegwerf main), dort darfst du Test-Dateien anlegen; am Ende Worktree und Zweig wieder entfernen. Umgebung: JAVA_HOME=/usr/lib/jvm/java-25-openjdk ANDROID_HOME=/opt/android-sdk ANDROID_USER_HOME=/tmp/claude-1000/-home-dysseus-Documents-Claude/a0d87cde-04f6-43b4-bc17-a2150aed9501/scratchpad/android-home; Submodul whisper.cpp per --reference von ~/Projects/Boulder. Bildtests nach dem Muster der vorhandenen Robolectric-Bildtests (GesundheitBildTest, HerzBildTest, WasserStackBildTest, SportUnterseiteTest, PulsKarteBildTest). Code-Landkarte: ${V1}/karte-0.51.md (mit Nachträgen 0.52). Designregeln der App: EIN Baukasten ansicht/Baukasten.kt, EINE Farbquelle ansicht/Farben.kt, EINE Formatquelle kiesel/Formate.kt + ansicht/Formathilfen.kt; Diagramme: Raster durchgezogen, Achse rechts, Ziel durchgezogen, Mittel gestrichelt, Zonen gepunktet; UI nur Englisch; keine Textwände (nur Zahlen, Urteile, kurze Labels). Freigegebener Entwurf des Health-Bereichs: ${V1}/entwurf-erlebnis/*.dc.html. Deutsch, knapp.`

phase('Screens')
const screens = await agent(`${GRUND}

Erfasse JEDEN Bildschirm der Boulder-App, der zu Boulders eigenen Teilen gehört (Health-Start mit Tagesband, Nacht, Heart & Body, Körperseite, Water, Stack, Sport und Unterseiten, Training-Detail, All workouts, Akku, alle Trends-Seiten (Herz, Schlaf, Ernährung, Sport, …), Einstellungen Health/Sport, Blätter/Dialoge wie Ändern/Löschen, Kieselsport-/Drinktervall-/SupCycle-Seiten, Leer- und Fehlerzustände) – Liste aus den Routen/Navigation ableiten. Rendere jeden mit realistischen Beispieldaten (Robolectric, echte Grafik, w412dp, dunkles UND helles Schema, lange Seiten in voller Höhe) als PNG nach ${V1}/screens-0.52/<bereich>-<name>[-hell].png. Schau jedes Bild selbst an (Read), verwirf leere/kaputte und mache sie richtig. Schreibe ${V1}/screens-0.52/index.md (Bildschirm → Datei → Route/Composable). Gib Liste und Lücken zurück (was sich nicht rendern liess und warum).`, { label: 'screens', phase: 'Screens' })

phase('Design-Prüfung')
const audit = await agent(`${GRUND}

Prüfe die Design-Konstanz über alle Bilder in ${V1}/screens-0.52/ (index.md) und den Code: Kartenstil (Rand, Radius, Abstände), Titel/Kopfzeilen, Typografie (Grössen, Gewichte), Farben (nur aus Farben.kt, Bedeutung gleich: Herz rot, HRV violett, Schlaf blau …), Diagrammregeln, Formate (Zahlen, Einheiten, Datum/Zeit), Leerzustände, Navigation (Zurück, Titel), Begriffe (gleiche Dinge gleich benannt), Textwände. Vergleiche mit dem Entwurf ${V1}/entwurf-erlebnis/. Besonders: die Trends-Seiten (der Nutzer sagt, sie passen nicht). Schreibe ${V1}/design-pruefung-0.52.md: Befunde nach Schwere (je mit Bild und Datei:Zeile), Tabelle "Abweichung → richtig wäre", und eine kurze Liste, was beim Entfernen der Trends überall wegfällt oder umgehängt werden muss (alle Links/Zeilen "Trends", Ernährungstrends, Sleep trends, Heart trends, Sport-Trends, Steps-Woche "Trends >" usw.). Gib eine Kurzfassung zurück.`, { label: 'design-pruefung', phase: 'Design-Prüfung' })

phase('Correlation')
const konzept = await agent(`${GRUND}

Arbeite ein Konzept für eine neue Seite "Correlation" aus, die alle Trends ersetzt. Wunsch des Nutzers: Er erstellt selbst Karten, auf denen ZWEI Werte direkt zusammen gezeigt werden. Beispiele des Nutzers: (1) Koffein-Konsum mit Halbwertszeit (aktives Koffein) zusammen mit dem Puls über den Tag inkl. Nacht in einem 24-h-Fenster; (2) Einnahme von Magnesium gegen kein Magnesium. Grundsatz: Koffein erscheint nur in Boulder (nie auf der Uhr); alle Daten, die Boulder hat (Health Connect inkl. fremder Apps, eigene DB: Nächte mit Phasen, HRV, SpO2, Puls je Minute, Schritte, Trainings, Getränke/Koffein, Präparate von SupCycle, Blutdruck, Gewicht/Körper, Wasser).
Inhalt von ${V1}/correlation-konzept.md:
- Kartentypen (mind. A "Überlagerung über die Zeit" mit Fenster 24 h/7 Tage, B "Gruppenvergleich" mit/ohne bzw. vorher/nachher, ggf. C "Verschiebung" z. B. Koffein am Abend → nächste Nacht), je mit Beispielen und genau welche Werte kombinierbar sind (Matrix Wert × Kartentyp mit Datenquelle, Auflösung, Verfügbarkeit, Mindestmenge).
- Ablauf zum Erstellen/Bearbeiten/Löschen/Ordnen von Karten (wenige Schritte, keine Textwände), Speicherung (eigene DB/Prefs), Leerzustände ("zu wenige Tage").
- Statistik ehrlich und einfach: Gruppengrösse, Mittel, Abstand, "no difference" grau, Mindestanzahl je Gruppe; keine Pseudo-Kausalität, keine Korrelationskoeffizienten-Textwände.
- Einbettung in Navigation (wo die Seite erreichbar ist, nachdem alle Trends entfallen), Design nach Baukasten/Farben/Diagrammregeln.
- Entwürfe: 4–6 Bildschirme als .dc.html-Dateien (Format wie ${V1}/entwurf-erlebnis/*.dc.html, 412 breit, dunkles Schema, Regeln in /tmp/claude-1000/-home-dysseus-Documents-Claude/a0d87cde-04f6-43b4-bc17-a2150aed9501/scratchpad/artifact-files/14f280b6-0e6a-4297-8553-71f8d363989b/artifact-type/reference/format.md): Correlation-Liste mit eigenen Karten, Karte erstellen (Werte wählen), Beispielkarte Koffein + Puls 24 h, Beispielkarte Magnesium ja/nein, Leerzustand. Ablage ${V1}/entwurf-correlation/ (nur Dateien schreiben, nicht veröffentlichen; der Hauptagent veröffentlicht).
Gib eine Kurzfassung zurück.`, { label: 'correlation-konzept', phase: 'Correlation' })

phase('Aufwand')
const aufwand = await agent(`${GRUND}

Lies ${V1}/correlation-konzept.md und ${V1}/design-pruefung-0.52.md. Schätze den Aufwand ehrlich, gestützt auf den Code: Arbeitspakete (Trends entfernen und Links umhängen; Datenzugriffe je Wert; Kartenmodell + Speicherung; Editor; Kartentypen A/B/C; Diagramme; Statistik; Tests/Bildtests; Design-Korrekturen aus der Prüfung), je Paket Umfang (Dateien/Zeilen grob), Risiko, Abhängigkeiten, und Dauer in "Agentenläufen" wie bei 0.51/0.52 (0.51 ≈ 3,3 h mit 8 Agenten, 0.52 ≈ 2 h mit 6). Prüfe dabei das Konzept adversarial (fehlende Daten, falsche Annahmen über Auflösung/Verfügbarkeit, Statistik-Fallen, Widerspruch zu Grundsätzen) und korrigiere es direkt (Abschnitt "Gegenprüfung" anhängen). Schreibe ${V1}/correlation-aufwand.md und gib Kurzfassung mit Gesamtschätzung und Vorschlag zur Reihenfolge (z. B. 0.53 Design-Korrekturen + Trends raus + Correlation Typ A/B; später C) zurück.`, { label: 'aufwand', phase: 'Aufwand', effort: 'high' })

return { screens, audit, konzept, aufwand }
