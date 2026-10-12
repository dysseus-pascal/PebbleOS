export const meta = {
  name: 'e2e-uhr-app',
  description: 'End-to-End: neue Firmware in QEMU emery mit neuer Boulder-App im Android-Emulator, Datenfluss und UI, Watchapps, DB-Umzug',
  phases: [
    { title: 'E2E', detail: 'zwei Agenten mit eigenen Emulatoren' },
    { title: 'Gegenpruefen', detail: 'Belege ansehen' },
  ],
}

const S = '/tmp/claude-1000/-home-dysseus-Documents-Claude/a0d87cde-04f6-43b4-bc17-a2150aed9501/scratchpad'
const GRUND = `UMGEBUNG
- Repos (nur lesen, nichts committen, nichts pushen): ~/Projects/Boulder (main), ~/Projects/pebbleos (main), ~/Projects/SupCycle (main), ~/Projects/Drinktervall (main). Bauen nur in eigenen Worktrees bzw. Ausgabeordnern unter ${S}/e2e/<dein-name>/ (git worktree add --detach ... main; Firmware-Submodule per Symlink aus ~/Projects/pebbleos einhaengen und am Ende aushaengen; Boulder-Submodul holen; ~/Projects/Boulder/local.properties fuer signierte Bauten kopieren, nie committen). Am Ende eigene Worktrees entfernen, Emulatoren beenden.
- Firmware: . ~/pebbleos-sdk-0.1.8/env.sh && . ~/Projects/pebbleos/.venv/bin/activate; qemu_emery-Bau und QEMU-Start siehe BOULDER.md, tools/ und die bisherigen Wege (pbl configure --board qemu_emery ...; QEMU mit eigenen, freien Ports; vor dem Start pruefen, dass der Port frei ist).
- Boulder: export JAVA_HOME=/usr/lib/jvm/java-25-openjdk ANDROID_HOME=/opt/android-sdk ANDROID_USER_HOME=${S}/e2e/<dein-name>/android-home. Eigenes AVD anlegen (system-images;android-36;google_apis;x86_64 oder was installiert ist; adb root geht bei google_apis). Debug-APK (:androidApp:assembleDebug) hat den QemuSetupReceiver: adb shell am broadcast -a coredevices.coreapp.ADD_QEMU_WATCH -n ch.dysseus.boulder/coredevices.coreapp.debug.QemuSetupReceiver --es host 10.0.2.2 --ei port <QEMU-Pebble-Protokoll-Port>. Eigene Emulator-Ports (emulator -port) waehlen, damit zwei AVDs nebeneinander laufen.
- Nichts loeschen ausser eigenen Dateien. Keine echten Konten, keine echten Nutzerdaten.
- Findest du einen Fehler: nicht beheben, sondern mit genauem Ablauf, Log-Auszug und Beleg (Datei) melden.
- Jeden Bildschirm, den du als Beleg nimmst, selbst ansehen (Read auf die PNG) und beschreiben, was zu sehen ist. Belege unter ${S}/e2e/<dein-name>/belege/.
NUTZERENTSCHEIDUNGEN zur Pruefung: Nacht auf der Uhr = ein Schlafblock, Schlafsumme ohne Wachminuten; eigene DLS-Tags 0x4001/0x4002 (App nimmt 88/89 weiter); Rueckfrage vor .pbw/.pbl; Health-App (Boulder) auf emery; Kieselsport Auto-Ende nach 30 min Pause; Drinktervall still nach Tagesziel.`

const SCHEMA = {
  type: 'object',
  properties: {
    pruefungen: { type: 'array', items: { type: 'object', properties: {
      name: { type: 'string' }, ergebnis: { type: 'string', enum: ['bestanden', 'gescheitert', 'nicht-moeglich'] },
      beleg: { type: 'string' }, details: { type: 'string' },
    }, required: ['name', 'ergebnis', 'beleg', 'details'] } },
    fehler: { type: 'array', items: { type: 'string' } },
    fazit: { type: 'string' },
  },
  required: ['pruefungen', 'fehler', 'fazit'],
}

const A = `E2E-A "Uhr <-> App Datenfluss". Dein Name fuer Ordner: e2e-a.
1. Firmware main fuer qemu_emery bauen, QEMU starten. Pruefe: Zifferblatt ist TimeStyle Bedrock (Bild), Health-App oeffnet mit drei Karten (Bild), Kieselsport-App startet (Bild).
2. Boulder-Debug-APK von main bauen, eigenes AVD, installieren, Onboarding durchlaufen (Rechte verweigern ist ok, wo noetig erlauben fuer Health Connect falls vorhanden), QEMU-Uhr per QemuSetupReceiver verbinden. Beleg: Uhr verbunden in der App (Bild), Log.
3. Nacht auf neuem Tag: In QEMU per gdb (wie BOULDER.md bzw. frueher: am Haltepunkt boulder_nacht_minute oder einer KernelBG-Stelle dls_create(0x4001, DATA_LOGGING_BYTE_ARRAY, 328, ...) und dls_log mit einem gueltigen Nacht-Satz (Fassung 3, Layout aus include/pbl/services/activity/boulder/nacht.h; Beginn z. B. gestern 22:00 UTC+2, plausible Abschnitte) aufrufen. Ebenso HRV 0x4002 (251 B). Dann pruefen: App empfaengt (logcat), ACK geht erst nach dem Sichern, Nacht steht in boulder.db (adb root, sqlite3 bzw. Datei ziehen und lokal lesen), Nachtansicht in der App zeigt sie (Bild).
4. Alter Tag 88 (324/328 B) wird weiter angenommen (gleiche Methode).
5. Unbekannter Tag 0x4003 bzw. kaputter Satz: landet in der Quarantaene (filesDir/dls-quarantaene), ACK, Log.
6. TimeStyle-Einstellung aus der App (Apps -> Faces -> TimeStyle Bedrock -> Settings) aendern (z. B. Farbe) und pruefen, dass das QEMU-Zifferblatt sich aendert (Bilder vorher/nachher), und dass die Vorschau in der App dazu passt.
7. Kieselsport: In QEMU ein Training starten, kurz laufen lassen, pausieren, beenden (Tasten per QEMU-Steuerung/pebble-tool emu-button). Pruefen: Zusammenfassung kommt in Boulder an (logcat), Sport-Ansicht zeigt das Training (Bild). Health-Connect-Eintrag nur pruefen, wenn Health Connect auf dem AVD verfuegbar ist.
Melde je Punkt bestanden/gescheitert/nicht-moeglich mit Beleg.`

const B = `E2E-B "App-UI, Watchapps, Datenbank-Umzug". Dein Name fuer Ordner: e2e-b.
1. Datenbank-Umzug mit gestellten Daten: Boulder v0.49.0 Release-APK (gh release download v0.49.0 -R dysseus-pascal/Boulder bzw. Remote aus git remote -v) auf eigenem AVD installieren, starten. Gestellte Nacht-Daten erzeugen: entweder per QEMU-Uhr (eigene QEMU-Instanz mit b24-Firmware aus dem Release dysseus-pascal/boulder-firmware, gdb dls_create(88,...)) oder direkt per adb root in libpebble3.db schreiben (Schema Version 50 aus 0.49). Dann den signierten Release-Bau von main (gleicher Schluessel ci/debug.keystore bzw. local.properties) drueber installieren (adb install -r). Pruefen: logcat "Umzug fertig", boulder.db enthaelt dieselben Zeilen (Zeile fuer Zeile), libpebble3.db user_version 1045, alte Tabellen unberuehrt, Nachtansicht vorher/nachher (Bilder).
2. Sideload-Rueckfrage: eine .pbw (z. B. drinktervall.pbw aus ~/Projects/Drinktervall nach pebble build) per Intent oeffnen (adb push + am start -a android.intent.action.VIEW -d file/content-URI -t application/octet-stream bzw. ueber den Dateimanager). Pruefen: Rueckfrage erscheint (Bild), Abbrechen installiert nichts, Bestaetigen installiert (Locker-Liste, Bild).
3. Watchapps mit QEMU-Uhr (neue Firmware main fuer qemu_emery, eigene QEMU-Instanz, per QemuSetupReceiver an dein AVD): Drinktervall und SupCycle (beide von main gebaut) ueber Boulder installieren. Drinktervall: ein Glas abhaken -> Boulder empfaengt (logcat), ACK, Eintrag in der App/Akte. SupCycle: Plan senden (Konfigseite oder Standardplan), ein Praeparat abhaken -> Boulder empfaengt (logcat). Bilder der Uhr.
4. Firmware-Hinweis "Mindest-App": nur wenn ohne echtes Release moeglich (z. B. Debug-Schalter oder Test-Endpunkt); sonst nicht-moeglich mit Begruendung (Unit-Tests decken es).
Melde je Punkt bestanden/gescheitert/nicht-moeglich mit Beleg.`

phase('E2E')
const ergebnisse = await parallel([
  () => agent(`${A}\n\n${GRUND}\n\nDeutsch, knapp.`, { label: 'e2e:datenfluss', phase: 'E2E', schema: SCHEMA }).then(r => r ? { teil: 'A', ...r } : null),
  () => agent(`${B}\n\n${GRUND}\n\nDeutsch, knapp.`, { label: 'e2e:ui-watchapps-umzug', phase: 'E2E', schema: SCHEMA }).then(r => r ? { teil: 'B', ...r } : null),
])
const alle = ergebnisse.filter(Boolean)

phase('Gegenpruefen')
const gegen = await agent(`Du bist Skeptiker. Zwei End-to-End-Agenten haben Uhr (QEMU) und Boulder-App (Android-Emulator) geprueft. Ihre Ergebnisse:\n${JSON.stringify(alle, null, 1)}\n\nPruefe fuer jede als "bestanden" gemeldete Pruefung den Beleg selbst: Bilder unter ${S}/e2e/ mit Read ansehen, Logs/DB-Abzuege lesen. Ist der Beleg duenn oder passt er nicht zur Aussage, stufe auf "nicht belegt" herab. Liste ausserdem alle gemeldeten Fehler mit deiner Einschaetzung (echt? Schwere? Release-blockierend?). Nichts aendern, keine Emulatoren starten. Deutsch, knapp.`,
  { label: 'gegen:e2e', phase: 'Gegenpruefen' })
return { ergebnisse: alle, gegenpruefung: gegen }
