export const meta = {
  name: 'garmin-import-verlaufsrecht',
  description: 'Garmin Import: Recht auf ältere Daten anfragen, damit der Abgleich Garmins frühere Sätze sieht; Tests, APK neu',
  phases: [{ title: 'Bau', detail: 'Recht, Anzeige, Tests, APK' }],
}
const PROJ = '/home/dysseus/Projects/Garmin-Uebergabe'
phase('Bau')
const r = await agent(`Projekt ${PROJ} (Garmin Import, Spezifikation /home/dysseus/Documents/Claude/boulder-v1/garmin-uebergabe-spezifikation.md, README im Projekt). Lokales Repo ohne Remote; nicht pushen, kein GitHub. Umgebung: JAVA_HOME=/usr/lib/jvm/java-25-openjdk ANDROID_HOME=/opt/android-sdk ANDROID_USER_HOME=/tmp/claude-1000/-home-dysseus-Documents-Claude/a0d87cde-04f6-43b4-bc17-a2150aed9501/scratchpad/android-home (vorher ~/Projects/Boulder/ci/debug.keystore dorthin kopieren). Bezeichner deutsch, Kommentare zum WARUM, UI Englisch, kein catch ohne Log. Keine echten Gesundheitsdaten ins Repo.

Entscheid des Nutzers (07.10.2026): Die App fragt ZUSÄTZLICH das Leserecht auf ältere Daten an (HealthPermission.PERMISSION_READ_HEALTH_DATA_HISTORY, Manifest android.permission.health.READ_HEALTH_DATA_HISTORY), damit der Abgleich auch Garmins eigene Health-Connect-Sätze sieht, die älter als 30 Tage vor der ersten Freigabe sind, und keine Doppel schreibt. Vorbild Boulder: ~/Projects/Boulder pebble/src/androidMain/kotlin/coredevices/pebble/kiesel/KieselRechte.kt (FUNKTIONSRECHTE / nurVerfuegbare mit HealthConnectFeatures.FEATURE_READ_HEALTH_DATA_HISTORY: nur anfragen, wo die Akte die Funktion kennt).
Umsetzen:
- Recht anfragen, wo verfügbar; ohne die Funktion oder ohne Freigabe läuft die App wie bisher und die Vorschau nennt weiter die Grenze (der bestehende Hinweis aus Commit "Vorschau nennt die Grenze …"). Mit Freigabe entfällt der Hinweis bzw. die Grenze, und der Abgleich liest den ganzen Zeitraum des Exports.
- Das Fehlen dieses Rechts darf das Schreiben NICHT sperren (es ist nur fürs Abgleichen), aber die Vorschau muss es ehrlich sagen.
- RechteWaechterTest und Testakte anpassen (11 Rechte mit dem Verlaufsrecht; keine anderen Arten, kein INTERNET), neue Tests: mit Verlaufsrecht sieht der Abgleich fremde Sätze vor der Grenze und schreibt sie nicht doppelt; ohne Funktion wird das Recht nicht angefragt; ohne Freigabe Hinweis wie bisher. Mutationen mit werkzeuge/mutationen.py ergänzen und laufen lassen.
- README und Spezifikation kurz nachtragen (Abschnitt Regeln).
- Alle Tests --rerun-tasks, :app:clean :app:assembleRelease, apksigner prüfen (Zertifikat 212e0ca4…), aapt2: Rechteliste melden. Committen (deutsch, Schlusszeilen: "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" und "Claude-Session: https://claude.ai/code/session_012RYr1ZRXBzb7Y9hLeMoBPc").
Antworte knapp: Commit, Tests (Zahl, Mutationen), APK (Pfad, Grösse, SHA-256, Zertifikat, Rechte), offene Punkte.`, { label: 'verlaufsrecht' })
return r
