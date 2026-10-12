export const meta = {
  name: 'schlaf-fitbit-vergleich',
  description: 'V2-Schlafanalyse auf vier Fitbit-Nächten Minute für Minute vergleichen, systematische Fehler finden, Regel nur mit getrennten Einstell-/Prüfnächten vorschlagen',
  phases: [
    { title: 'Fitbit digitalisieren', detail: 'Phasen aus den vier Fitbit-Bildern je Minute' },
    { title: 'Vergleich', detail: 'V2-Analyse rechnen, Minute für Minute gegen Fitbit' },
    { title: 'Regel', detail: 'Kandidaten einstellen und auf getrennten Nächten prüfen' },
    { title: 'Gegenprüfung', detail: 'adversarial' },
  ],
}
const ORT = '/home/dysseus/Documents/Claude/boulder-v2'
const N = `${ORT}/naechte`
const S = '/tmp/claude-1000/-home-dysseus-Documents-Claude/a0d87cde-04f6-43b4-bc17-a2150aed9501/scratchpad/schlafvergleich'
const GRUND = `Boulder V2 Schlafanalyse (Firmware ~/Projects/pebbleos, Zweig origin/v2; die Schlafregeln stehen in fw/services/activity/boulder/schlafanalyse.c, bytegleich zu b26). Massgebend: ${ORT}/entscheidungen.md, Abschnitt "Schlaf: Vergleich mit Fitbit (08.10.2026)": FITBIT IST BEZUG, KEINE WAHRHEIT; der Nutzer findet Boulders Werte grundsätzlich richtiger (Aufstehen 8.10. 06:00 = Boulder richtig, Fitbit 05:13 falsch). Notizen: ${N}/notizen.md. Vorarbeiten: ${ORT}/b26-auswertung.md (Harness probe/harness.c ruft die ECHTE schlafanalyse_werte() auf; Regel F6/F9: neue Regeln nur mit getrennten Einstell- und Prüfnächten; "eine Pulsbedingung trug nicht" fürs Einschlafen).
Nächte (Boulder-Minutendaten): ${N}/nacht-2026-10-07.csv (7.→8.10.), ~/Projects/pebbleos/tests/fixtures/activity/boulder/nacht-2026-09-30.csv (30.9.→1.10.), nacht-2026-10-03.csv (= Nacht 2.→3.10.), nacht-2026-10-05.csv (5.→6.10.); Fitbit-Bilder ${N}/fitbit-2026-09-30.png, fitbit-2026-10-02.png, fitbit-2026-10-05.png, fitbit-2026-10-07.png.
Arbeite in ${S} (flüchtig: Ergebnisse, die bleiben sollen, nach ${ORT}/schlafvergleich/ schreiben). NICHTS in den Repos ändern oder committen. Deutsch, knapp, mit Belegen.`

phase('Fitbit digitalisieren')
const fit = await agent(`${GRUND}

Digitalisiere die vier Fitbit-Bilder zu Minutenreihen: In jedem Bild gibt es die Zeilen Wach (rosa), Restlessness (grün, nur Hinweis), REM (hellblau), Light (blau), Deep (violett) über einer Zeitachse von "Time that you went to bed" bis "Time out of bed" (Zeiten unten im Bild, Summen je Zeile oben). Bestimme die Pixelgrenzen der Achse, lies je Pixelspalte die Phase aus den Farben, rechne auf Minuten um und schreibe je Nacht ${ORT}/schlafvergleich/fitbit-<datum>-phasen.csv (minute_ab_bettzeit, uhrzeit, phase: wach/rem/leicht/tief, unruhig 0/1). PRÜFE gegen die im Bild genannten Summen (z. B. 7.10.: wach 27 min, REM 2 h 17, leicht 4 h 22, tief 50 min) und nenne die Abweichung je Phase in Minuten. Bild 5.10.: Format kann anders sein – erst ansehen (Read). Gib je Nacht Summen digitalisiert vs. Bild und die Dateipfade zurück.`, { label: 'fitbit-digitalisieren', phase: 'Fitbit digitalisieren' })

phase('Vergleich')
const vergleich = await agent(`${GRUND}

Digitalisierte Fitbit-Reihen: ${fit ? String(fit).slice(0, 1500) : '(fehlen – zuerst in ' + ORT + '/schlafvergleich/ nachsehen)'}

Rechne die AKTUELLE V2-Schlafanalyse (origin/v2, eigener Worktree in ${S}, Harness nach Vorbild b26: probe/harness.c bzw. die Host-Test-Lader; Regeln NICHT nachbauen, den echten Code aufrufen) auf allen vier Nächten und vergleiche Minute für Minute mit Fitbit: Einschlafen/Aufwachen, Summen je Phase, Übereinstimmung und Kappa, Verwechslungsmatrix, und vor allem die ABSCHNITTE, in denen beide sich widersprechen (≥ 5 min), je mit Puls (Median, gegen Ruhepuls und Schlafmedian der Nacht), HRV (15-min-Werte, falls vorhanden), Bewegung (vmc, ruhige Minuten), Lagewechsel, Schritte, Lage in der Nacht. Unterscheide: wo stützen Puls/HRV/Bewegung Boulder, wo Fitbit, wo ist es offen. Schreibe ${ORT}/schlafvergleich/vergleich.md (Tabellen, knapp) und gib eine Kurzfassung der systematischen Muster zurück.`, { label: 'vergleich', phase: 'Vergleich' })

phase('Regel')
const regel = await agent(`${GRUND}

Ergebnis des Vergleichs: ${ORT}/schlafvergleich/vergleich.md (lesen). Kurzfassung: ${vergleich ? String(vergleich).slice(0, 2500) : ''}

Prüfe, ob es für ein systematisches Muster eine Regel gibt, die Boulder dort korrigiert, wo PHYSIOLOGIE (Puls/HRV/Bewegung) klar gegen Boulder spricht – z. B. 7.10.: Wachphasen mit Puls auf Schlafniveau und hoher HRV, die Fitbit als REM führt. Vorgehen streng getrennt: Kandidaten (höchstens 3) auf ZWEI Einstellnächten einstellen (Wahl begründen, vor dem Rechnen festlegen und notieren), auf den ZWEI anderen prüfen. Kriterien: Prüfnächte näher an Fitbit in den betroffenen Minuten, KEINE Nacht schlechter (Einschlafen/Aufwachen ±0, echte Wachphasen mit hohem Puls/Schritten bleiben wach), Aufwachen 8.10. bleibt 06:00. Rechne mit Kopien von schlafanalyse.c im Scratchpad über den echten Harness. Ergebnis nach ${ORT}/schlafvergleich/regel.md: je Kandidat Regel in Worten, Parameter, Einstell- und Prüfergebnis, Risiko; Empfehlung (auch "keine Regel" ist ein gültiges Ergebnis).`, { label: 'regel', phase: 'Regel' })

phase('Gegenprüfung')
const pruefung = await agent(`${GRUND}

Gegenprüfung (nichts ändern ausser einem Abschnitt "Gegenprüfung" am Ende von ${ORT}/schlafvergleich/regel.md): Prüfe Digitalisierung (Summen gegen Bild), Vergleich (echter Code statt Nachbau? Zeitachsen richtig ausgerichtet? Zeitzonen?), Regelsuche (wirklich getrennte Nächte, Parameter vorab festgelegt, kein Überanpassen, Prüfnächte nicht heimlich mit eingestellt), und ob die Empfehlung aus den Zahlen folgt. Gib eine Liste der Befunde zurück (leer, wenn sauber) und dein Urteil zur Empfehlung.`, { label: 'gegenpruefung', phase: 'Gegenprüfung', effort: 'high' })

return { fit, vergleich, regel, pruefung }
