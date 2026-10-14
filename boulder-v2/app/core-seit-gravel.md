# Core 1.15.0.1 (ec2156cc) gegenüber Gravels Basis 96eb261a

Bestandsaufnahme vom 14.10.2026 (Rechercheauftrag, nachgelesen). 150 Commits, linear, 26.08. bis
08.10.2026; Tags 1.11.0 bis 1.15.0.1. 687 Dateien (+56 329/−9 399), davon Ring/Index +21 292/−7 337,
libpebble3 +18 671/−716 (Plugins, Berechtigungen, JS, test-apps +13 317). Grob: 65 Ring/Index-, 19
Plugin/Berechtigungs-, 13 Sprach- (Cactus), 12 Build/CI- und rund 40 Uhr/App-Commits.

## 1. Bereiche mit Berührung zu Boulder

### Health, Datalogging
- **90b9caa3:** Schritt-Parser liest Versionen über 13 bei `sample_size` ≥ 16; Overlay ohne Bit 0
  wird verworfen; active/resting kcal getauscht. Boulders Parser deckt das ab; die Datei kollidiert.
- **2a625912:** Datalogging fremder Watchapps geht an PebbleKit-2-Begleiter (`ThirdPartyDatalogEvent`).
  Health-Tags nur noch bei `SYSTEM_APP_UUID`; `logData`/`closeSession` mit timestamp, uuid, serial.
  Der ACK geht **weiter vor** der Verarbeitung raus; **unbekannte System-Tags werden bestätigt und
  verworfen** – ohne Boulders Zweig gingen Tag 88 und 0x4001/0x4002 verloren.
- **e501ff12:** „Imperial“ global, auch fürs Wetter.
- Health Connect und `HealthDataProcessor` unverändert; keine neuen Health-Tabellen.

### Datenbank 45 → 49 (alles AutoMigrations)
| Fassung | Commit | Änderung |
|---|---|---|
| 46 | 0f8e8bb2 | `CalendarEntity.writable` |
| 47 | 1fbc4165 | `LockerEntryEntity.pluginManifest`, `configPage` |
| 48 | cbb88032 | neue Tabelle `AppPermission`; `LockerAppPermission` wird **gelöscht** (`AutoMigration48`); `LockerEntryEntity.manifestVersion` |
| 49 | bbe5e35b | `AppPermission.seenAtMs` |
`coreapp.db` (util) 9 → 10 mit `sync_tasks` (Index). Beide mit `fallbackToDestructiveMigrationOnDowngrade`.

### Firmware
- **333877b8 „Remove Memfault OTA“:** Core-Uhren fragen immer `dash.repebble.com/api/ota/latest`,
  ältere die Cohorts. Boulders GitHub-Route in `FirmwareUpdateCheck` ist neu einzusetzen.
- Update-Ablauf in libpebble3 unverändert.

### Einstellungen
- `Section`, `TopLevelType` unverändert; `WatchSettingsScreen.kt` von 7 Commits geändert (+149/−62).
- Neu: „System Speech Model“, Debug „Use experimental plugins / permissions“, „Ask before (older) apps
  use permissions“, „Legacy PebbleKit“; Uhr: Ruhezeit-Pläne (`TypeSchedule`), Windeinheit.

### Begleiter, PKJS, Locker, Plugins (19 Commits)
- **Plugins:** JavaScript auf dem Telefon (in einer .pbw), mit Sources/Actions; experimentell,
  `enablePlugins = false`. OAuth über `appstore-api.repebble.com`, an die Firebase-uid gebunden.
- **Berechtigungen:** `usesPermissions` (manifestVersion 2), durchgesetzt nur mit `enablePlugins`;
  sonst bekommt jedes PKJS Netz und Standort.
- PKJS-Laufzeit umgebaut (`JsEngine`, XHR/fetch/WebSocket in commonMain, `startup.js`).
- Konflikte mit Boulder: `CompanionAppLifecycleManager`, `PKJSApp`, `Locker.kt`, `LockerScreen`,
  `LockerUtil`; Gravels PKJS-Schutz trifft genau die umgebauten Dateien.

### Uhr-Einstellungen, Timeline, Wetter
- `WatchPrefEntity.kt` (+106/−18): Schedule-Prefs, Windeinheit, Sprachcodes 2–9 entfernt. Cores
  `WatchSettingsPlugin` hat ein erschöpfendes `when` über alle `WatchPref`-Typen (braucht einen Zweig
  für Boulders `BytesWatchPref`).

### Konfliktfläche
196 Core-Dateien ausserhalb von Ring/Cactus/iOS/test-apps geändert: 96 in Boulder unverändert, 89
weichen ab (38 davon mit Boulder-Kommentaren), 11 fehlen in Boulder.

## 2. Was ein Android-Build von Core braucht

| Teil | Anmerkung |
|---|---|
| `google-services` 4.5.0 | `google-services.json` Pflicht; Attrappe mit `package_name coredevices.coreapp` (anpassen) |
| Crashlytics 3.0.7 + NDK | Mapping-Upload bei debug/`LOCAL_RELEASE_BUILD` aus |
| Firebase (gitlive 2.5.0): auth, firestore, crashlytics, storage | |
| FCM über `kmpnotifier` 1.6.1 | |
| `play-services-auth`, `googleid`, `credentials` | Google-Login |
| Play In-App-Update 2.1.0 | |
| health-kmp 1.4.0 (zieht `play-services-auth`, `play-services-fitness`) | Gravel schliesst beide aus |
| Mixpanel 8.9.0 | Analytics |
| `libcactus_engine.so` (68 MB, arm64) | proprietär; Telemetrie-Schnittstelle (`initTelemetry`) |
| `models/needle-pebble-ft-cq4.zip` (13,7 MB) | als Asset im APK |
| haversine-android | AAR mit nativen Bibliotheken, POM: Apache-2.0 |
| python3 | Pflicht (`scripts/pack-plugin-pbw.py`) |

Keine internen, nicht öffentlichen Artefakte gefunden. Tokens in `gradle.properties` (`bugUrl`,
`tokenUrl`, `mixpanelToken`, `memfaultToken`, `googleClientId` …) schalten die Dienste ein; ohne sie
laufen die Kernfunktionen.

**Cactus-Lizenz** (LICENSE in `cactus-compute/cactus`, am 14.10. abgerufen): „All Rights Reserved“;
frei nur für Privatpersonen (nicht kommerziell), kleine Organisationen, Bildung, Gemeinnützige. Keine
freie Lizenz; Gravel hält sie für unvereinbar mit der GPLv3-Verbreitung. Ob sie für das eingecheckte
Binary gilt, ist unklar.

## 3. Version, IDs, Toolchain

- versionName aus dem Tag, versionCode aus dem Tag gepackt (1.15.0.1 → 11 500 001); ein Tag wie
  `v0.53.0` bricht den Build. Boulder behält `BOULDER_VERSION` und die Commit-Zahl.
- applicationId `coredevices.coreapp`; Signatur über `keystore.jks`, `LOCAL_RELEASE_BUILD` aus
  `local.properties`.
- min 26, target 36, compile 37; Gradle 9.6.1, AGP 9.3.1, Kotlin 2.4.10, Room 2.8.4, JDK 17
  (`jvmToolchain(17)`) – derselbe Stand wie Boulder heute.

## 4. Gravels Ent-Googlung, gemessen

| Naht | Umfang bei Gravel |
|---|---|
| `:firebase-stubs` + `SignedOutUsersDao` | 598 Zeilen (205 Tests) + 44 |
| Crashlytics/google-services raus | Build-Dateien, 3 Quelldateien |
| FCM raus, `LocalNotify` | Löschungen + 79 Zeilen |
| Login, Play-Update, Mixpanel raus | 4 Dateien, −198, −49 |
| health-kmp ohne GMS | wenige Zeilen |
| `:haversine-stubs` | 272 Zeilen |
| `experimental` (Ring) ausgehängt | ~340 + ~390 Testzeilen |
| Cactus → whisper.cpp | ~12 000 Zeilen |

Ohne den Sprach-Ersatz grob 1 600 neue Zeilen (davon ~700 Tests) plus Löschungen (bei Gravel 50
Dateien, 5 442 Zeilen).

## Nicht geprüft
Laufzeitverhalten von Firebase mit der Attrappe; ob die haversine-Stubs neue Member brauchen; ob die
Boulder-Firmware die Sprachcodes 2–9 kennt; ob die gefundene Cactus-Lizenz für das Binary gilt.
