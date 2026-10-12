export const meta = {
  name: 'hrv-luecken-untersuchen',
  description: 'Lesend untersuchen, warum auf Heart & Body nur 3 von 7 Nächten einen HRV-Punkt haben',
  phases: [{ title: 'Untersuchen', detail: 'zwei Blickwinkel parallel, nur lesen' }],
}

const LAGE = `Boulder 0.51.0 (~/Projects/Boulder, main f078f11b) zeigt auf Heart & Body die HRV der letzten 7 Nächte (Bandwoche, früher HrvWoche, Rechnung bandwoche in Gesundheitsrechnung.kt). Auf dem Telefon des Nutzers (06.10.2026 21:08) stehen nur 3 Punkte: We 64, Fr 55, Tu 50 ms ("7-night avg 56"), Th, Sa, Su, Mo fehlen. Die Uhr läuft mit Boulder-Firmware (~/Projects/pebbleos, main = b26; davor b25 seit etwa 04.10., b24 stürzte ab), Messstufe vermutlich 3 (HRV nachts alle 30 min). Die Nacht 5./6.10. (Di) hat HRV 50.
NUR LESEN: nichts in den Repos ändern, nichts committen. Tests darfst du laufen lassen, Experimente nur in einer Kopie unter /tmp/claude-1000/-home-dysseus-Documents-Claude/a0d87cde-04f6-43b4-bc17-a2150aed9501/scratchpad/hrv-untersuchung/.
Umgebung Boulder: JAVA_HOME=/usr/lib/jvm/java-25-openjdk ANDROID_HOME=/opt/android-sdk.
Antworte deutsch und knapp: jede mögliche Ursache mit Beleg (Datei:Zeile, Testlauf), ob sie zu genau diesem Muster passt, und welche Beobachtung auf dem Telefon sie bestätigt oder ausschliesst (z. B. Nachtansicht der fehlenden Nächte, Health Connect). Am Ende: wahrscheinlichste Ursache und Vorschlag.`

const SCHEMA = {
  type: 'object',
  properties: {
    ursachen: { type: 'array', items: { type: 'object', properties: {
      ursache: { type: 'string' }, beleg: { type: 'string' }, passtZumMuster: { type: 'string' }, pruefungAmTelefon: { type: 'string' } },
      required: ['ursache', 'beleg', 'passtZumMuster', 'pruefungAmTelefon'] } },
    wahrscheinlichste: { type: 'string' },
    vorschlag: { type: 'string' },
  },
  required: ['ursachen', 'wahrscheinlichste', 'vorschlag'],
}

phase('Untersuchen')
const [kette, zuordnung] = await parallel([
  () => agent(`${LAGE}

Blickwinkel KETTE: Verfolge den Weg eines HRV-Werts von der Uhr bis zum Punkt im Bild. Firmware (wann misst sie HRV in der Nacht, unter welchen Bedingungen entsteht kein Wert, wie wird er übertragen: 0x4001/0x4002, DLS, Health-Tag), App (Empfang, Parsen, eigene Room-DB seit 0.50, Schreiben nach Health Connect, Laden in Gesundheitsdaten.lade), Rechnung (welcher Wert wird je Nacht genommen). Wo kann eine Nacht ohne HRV enden?`, { label: 'kette', phase: 'Untersuchen', schema: SCHEMA }),
  () => agent(`${LAGE}

Blickwinkel ZUORDNUNG: Wie ordnet bandwoche Nächte den 7 Tagen zu (Aufwachtag, Einschlaftag, Zeitzone, Mitternacht), was passiert bei zwei Nächten an einem Tag, Nickerchen, sehr kurzen Nächten, Nächten ohne Phasen, Werten aus fremden Apps in Health Connect, und wie entsteht der 7-Nächte-Schnitt? Prüfe mit einem Test in der Kopie, ob bei 7 vorhandenen Nächten mit realistischen Zeiten (Einschlafen 21:30-23:30, Aufwachen 05:30-07:00) Punkte verloren gehen. Prüfe auch die Blutdruckwoche auf dieselbe Frage.`, { label: 'zuordnung', phase: 'Untersuchen', schema: SCHEMA }),
])
return { kette, zuordnung }
