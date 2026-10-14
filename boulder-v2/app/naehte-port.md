# Nähte: Boulders Haken in Core 1.15.0.1 (Portierungsleitfaden)

Stand 14.10.2026 (Rechercheauftrag, nur gelesen). Grundlage: `diff gravel033 → aecd3350` (Boulder), `96eb261a → ec2156cc` (Core);
für Firmware-Dateien zusätzlich Gravel-Tag `0.3.2`. **Zeilennummern = ec2156cc** (für alle Dateien hier gleich dem
Arbeitsbaum, ausser `HealthDataEntity.kt`, das AP1 gerade ändert). Nicht hier (andernorts): `Database.kt`,
`LibPebbleModule.kt`, `backup_rules.xml`, `full_backup_content.xml`, `Datalogging.kt`, `DataLoggingService.kt`,
`androidApp/build.gradle.kts`, `settings.gradle.kts`, Manifest von `androidApp`.

- Das [S]-„AndroidManifest.xml" aus Gruppe G ist **`pebble/src/androidMain/AndroidManifest.xml`**; das von `androidApp`
  ist zwischen Gravel 0.3.3 und Boulder 0.53 byte-gleich (Abschnitt G6).
- Alle 36 Dateien liegen in Core 1.15 am selben Pfad; nur `GithubReleases.kt` und `VerifiedFirmwareInstaller.kt` fehlen
  (Gravel-Dateien, K1/K2).
- „Gravel-Abhängigkeit" = Gravel-Code, ohne den Boulders Änderung nicht wirkt; alles andere aus Gravel bleibt weg.

## Querschnitt: so werden die Nähte kleiner

1. **Helfer raus aus Core-Dateien:** top-level/`internal`-Funktionen und Typen in Boulder-Dateien im selben Paket
   (`begleiterEingang`, `konfigurationsUrlErlaubt`, `systemAppsEinfuegen`, `hexAlsBytes`/`BytesWatchPref`,
   `wetterOrtNull`, `SideloadArt`, `firmwareHinweis`/`aufDerUhrText`, `faellig`/`nachholenBeimVerbinden`, SpO2-Value-Klassen).
   In Core bleibt nur die Aufrufstelle.
2. **Routen:** Boulders 18 Routen in eigenes `@Serializable`-Objekt + `NavGraphBuilder.addBoulderNavBarRoutes()`
   (Core: `composableWithAnimations` ist `public inline`) → 1 Zeile in `PebbleRoutes.kt`.
3. **Einstellungen:** Core hat `Section`/`rawSettingsItems` unverändert → Boulder hängt weiter über
   `boulderSettingsItems` an; SpO2-Einträge dorthin verlegen.
4. **TimeStyle Bedrock (F26)** trägt allein: `LockerScreen`, `LockerUtil`, `LockerAppScreen`, `MyCollectionScreen`,
   `BytesWatchPref`/`TypeBytes` (+ Cores `WatchSettingsPlugin`), `WatchPrefsScreen`-Filter, `Weather.wetterDerUhr`,
   `SystemApps.TimeStyleBedrock`, `TimeStyleBedrockSettingsRoute`, zwei Beobachter in `PebbleAppDelegate`. Entfällt TS,
   fallen diese Nähte weg.
5. **Firmware-Kanal (Gravel 0.3.2, „Early PebbleOS updates")** ist für `boulder-firmware`-Releases wirkungslos
   (Boulder-Fassungen „warten nicht", `selectRelease`) → fest `Soaked`, `CoreConfig.firmwareUpdatesEarlyChannel` und
   Debug-Schalter entfallen.

---

## A Nächte/Minutendaten

### A1 HealthDataProcessor.kt
- **1 Pfad:** `libpebble3/src/commonMain/kotlin/io/rebble/libpebblecommon/datalogging/HealthDataProcessor.kt`
- **2 Boulder** (Gravel: unverändert):
  - Neuer Eingang `suspend fun speichere(sessionId, tag, appUuid, itemSize, payload): Ablage` – erst lesen (`lies`),
    dann unter `schreibSperre` (Mutex) schreiben; `Ablage.Gesichert | Quarantaene(grund) | NichtGespeichert`. Erst danach
    darf die Uhr freigeben (DataLoggingService). Ersetzt `handleSendDataItems`/`processDataItem`/`pendingData`/
    `PendingDataItem` (Fire-and-forget `scope.launch`).
  - `handleSessionOpen`: `if (tag !in HEALTH_TAGS && BoulderDls.art(tag, applicationUuid, itemSize) == null) return`.
  - `liesBoulder()`: `BoulderDls.Art.NACHT/HRV` → `parseNachtData/parseNachtHrvData` → `nachtDao.speichereNachtTeil/HrvTeil`
    (Teil 0 räumt die vorige Fassung); `Art.UNBEKANNT` → Exception → Quarantäne.
  - Schritte: `ParsedHealthData` → zusätzlich `spo2Dao.insertSpo2Readings`.
  - `_healthDataUpdated` mit `extraBufferCapacity = 1, DROP_OLDEST`; `Schubsammler` meldet einmal je Schub nach
    `aktualisiereHeute()` (statt `itemsLeft == 0` – Firmware schickt dort 0xffff; plus stündlich erneut).
  - Konstruktor + `spo2Dao, nachtDao, clock`; `emitHealthDataUpdated()` entfällt.
- **3 Core:** unverändert seit 96eb261a.
- **4 Anker:** ganze Klasse Z. 34–245 (`handleSessionOpen` Z. 62, `handleSendDataItems` Z. 80, `processDataItem` Z. 93,
  `processHealthData` Z. 162, `processStepsData` Z. 189, `processOverlayData` Z. 223, `PendingDataItem` Z. 242).
  Kein Haken in Core. Empfehlung: Datei als Boulder-Ersatz führen (Core hat sie in 150 Commits nicht angefasst) –
  Alternative: Logik in Boulder-Klasse, `HealthDataProcessor` delegiert (~15 Zeilen Naht).
- **5 Risiko:** Cores `Datalogging` filtert in `logData` Z. 38–43, `openSession` Z. 97–100, `closeSession` Z. 111–114 auf
  `SYSTEM_APP_UUID && tag in HEALTH_TAGS` → `BoulderDls`-Zweig wirkt erst mit Datalogging-Umbau. `emitHealthDataUpdated`
  wird von Cores `Health.populateDebugHealthData()` (Z. 437) gerufen → dort mit entfernen (C2). DI-Konstruktor in
  LibPebbleModule.

### A2 HealthDataParser.kt
- **1 Pfad:** `libpebble3/src/commonMain/kotlin/io/rebble/libpebblecommon/health/parsers/HealthDataParser.kt`
- **2 Boulder** (Gravel: unverändert):
  - `parseStepsData(payload, itemSize, verworfen = {}): ParsedHealthData` (statt `List<HealthDataEntity>`); je Item ein
    Teilpuffer, je Minute `sample_size` Bytes; Tabelle `MINUTE_BYTES` (5→6, 6→12, 7→13, 12→15, 13→16, 14→18); unbekannte
    Fassung oder zu kleines `sample_size` → Item weg + `verworfen(grund)` (→ Quarantäne). v14: `spo2Percent/spo2Quality`
    → `Spo2ReadingEntity`, wenn > 0.
  - `VERSION_FW_4_1 = 8` → `VERSION_HR_GEWICHT = 12` (Firmware zählt 4,5,6,7,12,13,14).
  - Overlay: `verworfen`-Callback; Schrittdaten auch für `OpenWorkout`/`Movement`; Reihenfolge aktiv/Ruhe-kcal korrigiert;
    `distanceM`; Boulder-Sitzungsfassung 7 mit `satzGroesse ≥ 27`: Byte 26 = `manual`.
- **3 Core (90b9caa3):** Schritt-Parser vorwärtskompatibel (Version > 13 bei `sampleSize ≥ 16`, Rest je Sample
  überspringen); Overlay mit gelöschtem Bit 0 der Version verworfen; aktiv/Ruhe getauscht; neuer
  `HealthDataParserTest` (108 Z.).
- **4 Anker:** `parseStepsData` Z. 26, `parseOverlayData` Z. 162 (`compatibleVersion` Z. 190–191), Konstanten ab Z. 251.
  Boulder deckt Cores Fix 1+3 ab; Cores Bit-0-Prüfung in Boulders Overlay-Zweig übernehmen (Boulder-Fassung 7 ist
  ungerade → bleibt lesbar). Naht = ganze Datei.
- **5 Risiko:** Cores `HealthDataParserTest` bricht: `.size/.map` auf `ParsedHealthData` (→ `.healthData`);
  `parsesNewerVersionWithLargerSamples` füllt v14 mit 0xAA → Boulder speichert SpO2 = 170 % (Core: v14 = „unbekannt neuer",
  Boulder: v14 = SpO2; Plausibilitätsgrenze ≤ 100 erwägen); `skipsOverlayVersionWithBit0Cleared` ohne Bit-0-Regel rot;
  `distanceCm` (A4). Unbekannte Fassungen: Core rät (v15+), Boulder quarantäniert – bewusst Boulder, Test anpassen.

### A3 HealthRecords.kt
- **1 Pfad:** `libpebble3/src/commonMain/kotlin/io/rebble/libpebblecommon/health/HealthRecords.kt`
- **2 Boulder:** `OverlayType.Movement(8)` – erkannte Bewegung der Boulder-Firmware (≥ 10 min), kein Training.
  Gravel: nichts.
- **3 Core:** unverändert.
- **4 Anker:** `enum class OverlayType` Z. 9, nach `OpenWorkout(7);` Z. 16. 2–4 Zeilen, kein Haken.
- **5 Risiko:** gering; Core-`when`s haben `else` (PlatformHealthSync Z. 229/240, HealthCharts Z. 125). `fromValue(8)`
  liefert jetzt Movement statt null → Cores Health-UI zeigt es als „sonstige" Sitzung.

### A4 HealthDataEntity.kt (OverlayDataEntity)
- **1 Pfad:** `libpebble3/src/commonMain/kotlin/io/rebble/libpebblecommon/database/entity/HealthDataEntity.kt`
- **2 Boulder:** `distanceCm` → `@ColumnInfo(name = "distanceCm") val distanceM` (Firmware schickt Meter); neu
  `@ColumnInfo(defaultValue = "0") val manual: Boolean = false`. Gravel: nichts.
- **3 Core:** unverändert.
- **4 Anker:** `data class OverlayDataEntity` Z. 27–36. **Nur `manual` ergänzen, Feldnamen `distanceCm` behalten**
  (`distanceM` liest ausser Parser und Tests niemand) → keine Brüche in Cores `Health.kt` (Z. 371/428),
  `HealthDataParserTest`, Queries. Arbeitsbaum (AP1) ist bereits so.
- **5 Risiko:** Spalte `overlay_data.manual` gehört zur DB-Kette 1045ff (Database.kt, andernorts).

## C HealthDataApi

### C1 LibPebble.kt
- **1 Pfad:** `libpebble3/src/commonMain/kotlin/io/rebble/libpebblecommon/connection/LibPebble.kt`
- **2 Boulder:**
  - Typen `LatestSpo2`, `Pulsreihe(zeiten: LongArray, werte: IntArray)`.
  - `HealthDataApi` +`getPulsMinuten`, `getSchritteJeHalbstunde` (mit Default-Rumpf), `getSpo2Readings`,
    `getSpo2ReadingsAfter`, `getNaechte`, `getNachtAbschnitte` (2×), `getNachtHrv`, `setzeNachtHrvWennLeer`, `setzeZiele`,
    `getAverageSpo2`, `getLatestSpo2Reading`; **`populateDebugHealthData()` entfernt** (löschte echte Daten;
    `DebugDatenTest` wacht).
  - `Weather.wetterDerUhr(): WeatherAppEntry? = null` (TimeStyle-Vorschau).
  - `FirmwareUpdateCheckResult.BrauchtNeuereApp(mindestApp)`.
  - `create(…, builtInCompanions: BuiltInCompanions = NoBuiltInCompanions)` → `initKoin(…, builtInCompanions)`.
  - Gravel (nicht übernehmen): `WatchappPermissions`-Delegation, `networkDenyEnforcement`-Parameter.
- **3 Core:** neue Schnittstellen `Plugins`, `AppPermissionsApi`; `HealthApi.updateImperialUnits`;
  `Weather.currentWeather: Flow<List<WeatherAppEntry>>`; Calendar (`createEvent(calendarId, …)`, `defaultCalendar`);
  LockerApi (`sideloadApp(path, loadOnWatch, granted)`, `reviewSideload/reviewInstall`, `appConfigPageUrl`);
  `create(…, pluginOAuthApi)`.
- **4 Anker:** `LatestHeartRate` Z. 158; `interface HealthDataApi` Z. 163–213 (`populateDebugHealthData` Z. 212);
  `interface Weather` Z. 215; `FirmwareUpdateCheckResult` Z. 301–318; `create(` Z. 579 (`pluginOAuthApi` Z. 592,
  `initKoin` Z. 594 – `builtInCompanions` dahinter). Kleinere Naht: neue Methoden + Typen in eigenes Interface
  (`BoulderGesundheitApi`, Boulder-Datei) und `interface HealthDataApi : BoulderGesundheitApi` – 1 Zeile.
  `currentWeather` ersetzt `wetterDerUhr` nicht (keine Uhr-Reihenfolge).
- **5 Risiko:** `FakeLibPebble` (Z. 551) muss alle neuen Methoden liefern; `WatchSettingsScreen` Z. 462 ruft
  `populateDebugHealthData`. Kein erschöpfendes `when` über `FirmwareUpdateCheckResult` in Core (nur `is`) →
  `BrauchtNeuereApp` gefahrlos. watchModule übergibt `create` positional → Boulder-Parameter benannt anhängen.

### C2 Health.kt
- **1 Pfad:** `libpebble3/src/commonMain/kotlin/io/rebble/libpebblecommon/health/Health.kt`
- **2 Boulder:** Konstruktor +`spo2Dao, nachtDao, nachtUmzug`; `init()` startet `nachtUmzug.sicherstellen()`;
  `bereich = libPebbleCoroutineScope.datenbereich("Health")` statt `libPebbleCoroutineScope.launch` (DB-Fehler beendeten die
  App) in `updateHealthSettings`, `sendHealthAveragesToWatch`, `startPeriodicStatsUpdate`; try/catch um
  `requestHealthData` und den 7-Uhr-Lauf; Implementierungen der neuen API (Spo2Dao, NachtDao,
  `HealthDao.getPulsMinuten/getSchritteJeHalbstunde`, `BoulderZiele.zeile`); `populateDebugHealthData` entfernt;
  `data class HealthSettings` +`bloodOxygenEnabled`, `bloodOxygenActivityEnabled`, `spo2MeasurementInterval`.
  Gravel: nichts.
- **3 Core:** `updateImperialUnits()` (→ `setImperialUnits`).
- **4 Anker:** `class Health(` Z. 44 (Parameter bis Z. 58), `init()` Z. 65, `updateHealthSettings` Z. 69,
  `requestHealthData` Z. 123, `sendHealthAveragesToWatch` Z. 130, `startPeriodicStatsUpdate` Z. 136 (Lauf Z. 161),
  `getHealthDataForRange` Z. 195, `getHRZoneMinutes` Z. 234, `populateDebugHealthData` Z. 285–438,
  `data class HealthSettings` Z. 441–461. Kleinere Naht: Boulder-Methoden per Delegation
  (`BoulderGesundheitApi by boulderGesundheit`, eigene Klasse).
- **5 Risiko:** HealthSettings-Felder ohne Default brechen jeden Konstruktoraufruf (`FakeLibPebble` Z. 386,
  `HealthSettings.kt` Z. 126) → Defaults `false, false, TenMin`. NachtDao/NachtUmzug kommen aus boulder.db (DI andernorts).

## D SpO2-Einstellungen

### D1 BlobDB.kt (endpointmanager)
- **1 Pfad:** `libpebble3/src/commonMain/kotlin/io/rebble/libpebblecommon/connection/endpointmanager/blobdb/BlobDB.kt`
- **2 Boulder:** `effectiveDatabaseFor()` leitet `bloodOxygenPreferences`, `bloodOxygenActivityPreferences`,
  `spo2Preferences` (von der Uhr über WatchPrefs) nach `BlobDatabase.HealthParams`. Gravel: nichts.
- **3 Core:** unverändert.
- **4 Anker:** `effectiveDatabaseFor` Z. 305–315, Zweig `"heartRatePreferences" -> BlobDatabase.HealthParams` Z. 312.
  3 Zeilen.
- **5 Risiko:** gering.

### D2 HealthSettingsRealDao.kt
- **1 Pfad:** `libpebble3/src/commonMain/kotlin/io/rebble/libpebblecommon/database/dao/HealthSettingsRealDao.kt`
- **2 Boulder:** `handleWrite` dekodiert die drei SpO2-Schlüssel (`BloodOxygen*BlobItem`, `Spo2PreferencesBlobItem`;
  Intervall nur 0..3, sonst `TenMin`). Gravel: nichts.
- **3 Core:** unverändert.
- **4 Anker:** `handleWrite` Z. 32; neue Zweige nach `"heartRatePreferences"` Z. 85, vor `else` (Z. 101). ~25 Zeilen.
- **5 Risiko:** gering; Typen aus D3.

### D3 HealthSettings.kt (entity)
- **1 Pfad:** `libpebble3/src/commonMain/kotlin/io/rebble/libpebblecommon/database/entity/HealthSettings.kt`
- **2 Boulder** (SpO2 aus coredevices/mobileapp#267, in Core nicht gemergt):
  - `key()`: `SFixedString` → `SNullTerminatedString(StructMapper(), id)` (Schlüssel mit NUL wie WatchPrefs; sonst
    schreibt die Uhr in den Flash, der Live-Wert bleibt alt).
  - `value()`: 3 Schlüssel; `getWatchSettings()`: 3 Flows als Triple (combine ≤ 5); `setWatchSettings()`: 3 Einträge.
  - Neue Typen `BloodOxygenPreferencesValue`, `BloodOxygenActivityPreferencesValue`, `Spo2PreferencesValue` + je
    `*BlobItem` (1 Byte). Gravel: nichts.
- **3 Core:** `HealthSettingsEntryDao.setImperialUnits()` (Z. 149).
- **4 Anker:** `key()` Z. 58–62, `value()` Z. 64–75, Schlüssel Z. 82–85, `getWatchSettings` Z. 107 (`combine` Z. 120),
  `setWatchSettings` Z. 159, BlobItems ab Z. 299. Kleinere Naht: Value-/BlobItem-Klassen in Boulder-Datei
  (gleiches Paket; `json` ist `private` → eigenes `Json`).
- **5 Risiko:** NUL-Schlüssel gilt für **alle** Health-Einstellungen, auch an Core-Firmware – Protokolländerung,
  im Wächter markieren.

## E Einstellungen

### E1 WatchSettingsScreen.kt (+ .android.kt / .ios.kt)
- **1 Pfad:** `pebble/src/commonMain/kotlin/coredevices/pebble/ui/WatchSettingsScreen.kt`;
  `pebble/src/{androidMain,iosMain}/kotlin/coredevices/pebble/ui/WatchSettingsScreen.{android,ios}.kt`
- **2 Boulder:**
  - `enum class Section`: Health-Icon `Favorite`; neu `Sport`, `Navigation`, `Boulder`; Battery-Text wieder „Battery"
    (in Core schon so – keine Naht).
  - `navigatesDirectlyTo`: `Section.Battery -> PebbleNavBarRoutes.AkkuRoute` (eigene Akkuseite aus den Nächten).
  - `settingsBadgeTotal()`: `+ boulderAbzeichen()`.
  - SpO2: 3 `SettingsIds`, 3 Einträge nach „HR During Activities" (dessen Aus schaltet `bloodOxygenActivityEnabled` mit aus);
    `basicSettingsToggleItem(…, enabled: Boolean = true)`.
  - `rawSettingsItems = rawSettingsItems + boulderSettingsItems(navBarNav)`.
  - Entfernt: HealthStatsDialog, „View debug stats", „Populate fake health data" (+Dialog), „Open Google Fit"
    (+`expect/actual openGoogleFitApp`), „Show Health Tab", `SettingsItemsState.healthTrackingEnabled`,
    „displaced"-Einträge.
  - Sektionsabzeichen `badge?.toIntOrNull() ?: 1` („!" zählt); Spacer in den Badge-Block; „Please update the Boulder app!".
  - Gravel-Rest, den Boulder behält: Debug „Early PebbleOS updates" (0.3.2). Alles Übrige aus Gravel (whisper,
    SelfHostedServerDialog, WhatsNew, Watch App Permissions, Battery-Sperre) **nicht** übernehmen.
- **3 Core (7 Commits, +149/−62):** „Imperial Units" nach General (`updateImperialUnits` + Wetter neu), Wetter-„Units" und
  Debug „Use Core OTA service" weg; „System Speech Model"; Debug Plugins/Rechte, „Ask before (older) apps…",
  „Legacy PebbleKit"; Abmelden über `usersDao`.
- **4 Anker:** Section Z. 211 (Battery Z. 219, Health Z. 221), `navigatesDirectlyTo` Z. 236–239, `SettingsIds` Z. 241,
  `settingsBadgeTotal` Z. 279–297, Dialog-States Z. 357–358 und Z. 450–465, „HR During Activities" Z. 1155,
  „Open Google Fit" Z. 1310, „Show Health Tab" Z. 1320, „View debug stats" Z. 1331, „Populate fake…" Z. 1342,
  `SettingsItemsState(` Z. 2121/2125, displaced Z. 2285, Badge-Summe Z. 2326, Spacer Z. 2442,
  `basicSettingsToggleItem` Z. 2475, `expect fun openGoogleFitApp` Z. 3008.
  Kleinere Naht: (a) SpO2-Einträge in `boulderSettingsItems` (erscheinen dann am Ende von Health); in Core bleiben die
  3-Zeilen-Kopplung und der `enabled`-Parameter; (b) displaced/`healthTrackingEnabled`/„Show Health Tab" unangetastet
  lassen (nur mit `enableIndex`, Vorgabe false); (c) „Open Google Fit" per `show = { false }` statt Löschen samt
  expect/actual; (d) Early-Kanal weglassen; (e) Boulders eigener Imperial-Schalter in `BoulderSettings.android.kt`
  entfällt – Core zeigt „Imperial Units" jetzt immer (General).
- **5 Risiko:** Datei mit höchster Core-Änderungsrate. Entfernen von `populateDebugHealthData` zieht Eintrag + Dialog
  mit. Cores Imperial-Schalter schreibt nur den Einheiten-Eintrag; Boulders `Formate` liest `imperialUnits` → passt.

## F Health-Connect-Export

### F1 PlatformHealthSync.kt
- **1 Pfad:** `pebble/src/commonMain/kotlin/coredevices/pebble/health/PlatformHealthSync.kt`
- **2 Boulder** (Gravel: unverändert):
  - `internal expect suspend fun requestPlatformHealthPermissions(…)` – alle Rechte inkl. SpO2 in einem Dialog
    (actual: `HealthConnectPermissions.android.kt`).
  - `Einzellauf` statt `_syncing` (nie parallel, Nachlauf); `Rechteanlass` (Rechte-IPC nur bei Neuem/Start/Rückkehr);
    `berechtigt() == null` (Akte antwortet nicht) schaltet nicht ab; `requestPermissions(nurEinschalten = false)`.
  - Minuten in Tagesblöcken (`inTagesbloecken`); neu `syncOxygenSaturation()`, `syncNaechte()` (Firmware-Nächte mit
    Phasen, erneut bei geänderter Nacht), `gibtNeues()`.
  - `syncOverlays`: Training nur `OpenWorkout` (kein Walk/Run), `manual`-Overlays übersprungen (Kieselsport trägt selbst
    ein), Schlaf in Firmware-Nächten unterdrückt (`ohneFirmwareNacht`), Schlaf wartet, wenn boulder.db unlesbar.
- **3 Core:** unverändert.
- **4 Anker:** `expect`s Z. 30–32, Klasse Z. 34, `_syncing` Z. 43, `startAutoSync` Z. 47 (Collect Z. 55),
  `requestPermissions` Z. 76, `sync()` Z. 111, `syncStepsAndHeartRate` Z. 130, `syncOverlays` Z. 183
  (`sleepTypes` Z. 185, `exerciseTypes` Z. 191, Schleife Z. 223). Kein Haken; als Boulder-Ersatzdatei führen, Helfer
  (`SCHLAF_OVERLAYS`, `syncNachFrage`, `ohneFirmwareNacht`) in Boulder-Datei.
- **5 Risiko:** braucht `HealthSyncTracker`-Felder (Boulder, nicht [S]) und Boulder-Dateien `Einzellauf`,
  `Rechteanlass`, `Tagesbloecke`, `NachtPlatform*`, `OxygenSaturationPlatform*`. Core-Aufrufer `WatchSettingsScreen`
  Z. 1282 bleibt dank Default kompatibel. Core erzeugt den HealthManager mit `HealthManagerFactory().createManager()`
  (watchModule Z. 150; Gravels GMS-freie Variante fehlt → F25).

## G Navigation, App-Start, Manifest

### G1 PebbleRoutes.kt
- **1 Pfad:** `pebble/src/commonMain/kotlin/coredevices/pebble/ui/PebbleRoutes.kt`
- **2 Boulder:** 18 `NavBarRoute`s (`TimeStyleBedrockSettingsRoute`, `TrainingRoute`, `TrainingDetailRoute(id)`,
  `TrainingAlleRoute(tag)`, `SportTrendRoute`, `SpurenRoute`, `GesundheitRoute`, `NachtRoute(beginnUtc)`,
  `TrendRoute(gruppe)`, `HerzRoute`, `WasserRoute`, `StackRoute`, `KoerperRoute`, `SicherungRoute`, `KieselsportRoute`,
  `ZieleRoute`, `ProtokollRoute`, `AkkuRoute`) + Composables; `HealthRoute`/`HealthScreen` entfernt. Gravel-Picker
  (`FirmwarePickerRoute`) entfernt; Gravels `WatchappPermissionsRoute` nicht übernehmen.
- **3 Core:** `PebbleRoutes.WatchappSettingsRoute` → `CommonRoutes.WatchappSettingsRoute(uuid, title)`.
- **4 Anker:** `object PebbleNavBarRoutes` Z. 51 (HealthRoute Z. 72), `composableWithAnimations` Z. 121,
  `addNavBarRoutes` Z. 159 (HealthRoute-Composable Z. 190). Kleinere Naht: Routen + `addBoulderNavBarRoutes(nav,
  topBarParams, viewModel)` in Boulder-Datei, 1 Aufruf in `addNavBarRoutes`. `HealthRoute` behalten (Core nutzt ihn:
  WatchHomeScreen Z. 811, WatchSettingsScreen Z. 2315).
- **5 Risiko:** ~35 Verweise `PebbleNavBarRoutes.<Boulder-Route>` mechanisch umbenennen (GesundheitScreens.android 13,
  TrainingScreens.android 5, BoulderSettings.android 4, SportUnterseiteTest 11, je 1 in Watches-/WatchSettings-/
  WatchHome-/LockerAppScreen).

### G2 WatchHomeScreen.kt
- **1 Pfad:** `pebble/src/commonMain/kotlin/coredevices/pebble/ui/WatchHomeScreen.kt`
- **2 Boulder:** Startreiter mit bekannter Uhr `Gesundheit` statt `WatchFaces`; Enum `Health(…, HealthRoute)` →
  `Gesundheit(Res.string.health, Icons.Outlined.FavoriteBorder, GesundheitRoute)`; `navBarEntries(indexEnabled)` =
  `[Gesundheit, WatchFaces, (Index), Watches, Notifications, Settings]` immer (ohne `healthEnabled`/`preferHealthTab`;
  ViewModel-Flows und `return` bei `healthEnabled == null` entfernt); Sideload-Dialog: Titel/Text je `SideloadArt`.
  Gravel (`searchState?.submit()`) nicht nötig.
- **3 Core:** `SecondaryProfileWarningDialog`; Deep-Link `WatchfacesRoute → WatchFaces`.
- **4 Anker:** `selectedTab` Z. 160–166 (Z. 164), Flows Z. 180–185, Gate Z. 261–264, `navBarEntries`-Aufrufe Z. 266/599,
  Sideload-Dialog Z. 334–350, Enum Z. 811, `navBarEntries` Z. 824–835. Kleinere Naht: Enum-Eintrag `Health` behalten, nur
  Icon + Route (`BoulderRoutes.GesundheitRoute`); in `navBarEntries` `mutableListOf(Health, WatchFaces)` und die beiden
  Bedingungen streichen (≈ 3 Zeilen); Startreiter 1 Zeile; Cores Gate darf bleiben (wartet nur auf den ersten Wert).
- **5 Risiko:** Dialogtexte hängen an `SideloadArt` (M1).

### G3 WatchesScreen.kt
- **1 Pfad:** `pebble/src/commonMain/kotlin/coredevices/pebble/ui/WatchesScreen.kt`
- **2 Boulder:** Menüpunkt „Battery" aktiv → `AkkuRoute`; Firmware-Hinweis `firmwareHinweis(firmwareUpdateAvailable)`
  (`FirmwareHinweis.Fehler` → rote Karte, `.Stand` → neutraler Text, z. B. „needs Boulder x.y"); Gravel-0.3.3-Picker raus;
  Farbe `boulderGruen`.
  **Gravel-Abhängigkeit:** `forkInstaller = koinInject<VerifiedFirmwareInstaller>()`, `installStateFor(watch)`,
  `stateTextSuffix()`, `ForkInstallProgressBar` (Gravel `ForkFirmwareInstallUi.kt`), Install-Knopf
  `forkInstaller.install(device, update)` statt `firmwareUpdater.updateFirmware(update)` – sonst keine Signaturprüfung.
- **3 Core:** Index-Kopplung/RSSI; `LanguageDialog` mit veröffentlichten Paketen (Download+Prüfung).
- **4 Anker:** Battery-Item Z. 1415–1427 (Route Z. 1425), `WatchDetails` Z. 2063, `firmwareUpdateInProgress` Z. 2080,
  `stateText` Z. 2085, Install-Bestätigung Z. 2179, `else if (… UpdateCheckFailed)` Z. 2197–2213.
- **5 Risiko:** Core zeigt „Battery Life" schon aktiv (Server-Seite `BatterySettingsRoute`) → nur Route. Farbe optional.

### G4 PebbleAndroidDelegate.kt
- **1 Pfad:** `pebble/src/androidMain/kotlin/coredevices/pebble/PebbleAndroidDelegate.kt`
- **2 Boulder:** Konstruktor +`context`, `healthDataApi`; `initPostPermissions()` einmal je Prozess (`boulderGestartet`):
  `Kieselstrasse.begleite`, Widget-Anstoss bei `healthDataUpdated`, `GesundheitWidget.sichereTakt`,
  `Rechtefrage.beimStart`, `Tagesarchiv.plane`, `Gesundheitsziele.beimStart`. Gravel: nichts.
- **3 Core:** unverändert.
- **4 Anker:** `initPostPermissions()` Z. 12–14 (Aufrufer `MainActivity` Z. 47, `androidDefaultModule` Z. 68). Kleinere Naht:
  `BoulderStart.nachRechten(context, libPebble, healthDataApi)` (Boulder-Datei mit eigener Einmal-Sperre) → 1 Zeile.
- **5 Risiko:** gering; `singleOf(::PebbleAndroidDelegate)` findet Context/HealthDataApi (watchModule Z. 155).

### G5 PebbleAppDelegate.kt
- **1 Pfad:** `pebble/src/commonMain/kotlin/coredevices/pebble/PebbleAppDelegate.kt`
- **2 Boulder:** `UhrNachfrage` (beim Öffnen frische Health-Daten; `beobachte(watches)`, `beimOeffnen()` direkt in
  `onAppResumed`, weil der `appResumed`-Sammler erst nach dem Onboarding steht); `TsBedrockNeusetzen` (nach FW-Update auf b23
  TS-Wert neu senden), `TsSprachpaketMerker`; `usersDao.updateLastConnectedWatch` nur bei neuer Seriennummer. Gravel
  (Crashlytics-Keys raus) nicht übernehmen.
- **3 Core:** `registerPlugin(weatherPlugin)`, `permissionRequestNotifier.init()`, `fetchIfNotFetchedYet()` beim Resume.
- **4 Anker:** Felder nach `logger` (Z. 72); neue `GlobalScope.launch` vor `analyticsEvents.collect` Z. 116; Watches-Sammler
  Z. 136 (`updateLastConnectedWatch` Z. 153); `onAppResumed()` Z. 247. Kleinere Naht: die drei Beobachter in eine
  Boulder-Klasse (1 Aufruf in `init`, 1 in `onAppResumed`).
- **5 Risiko:** Mit Firebase (F25) schreibt `updateLastConnectedWatch` nach Firestore → Dedupe bleibt sinnvoll.
  `TsBedrock*` entfällt mit F26.

### G6 pebble/src/androidMain/AndroidManifest.xml
- **1 Pfad:** `pebble/src/androidMain/AndroidManifest.xml`
- **2 Boulder:** Health-Connect-Rechte (WRITE/READ_OXYGEN_SATURATION, WRITE/READ_HEART_RATE_VARIABILITY, READ_EXERCISE,
  READ_HEART_RATE, WRITE/READ_HYDRATION, WRITE/READ_NUTRITION, WRITE_RESTING_HEART_RATE, WRITE_ACTIVE_CALORIES_BURNED,
  WRITE/READ_DISTANCE, WRITE_ELEVATION_GAINED, WRITE_SPEED, READ_BLOOD_PRESSURE, READ_WEIGHT, READ_BODY_FAT,
  READ_LEAN_BODY_MASS, READ_BONE_MASS, READ_BODY_WATER_MASS, READ_BASAL_METABOLIC_RATE, READ_HEIGHT,
  READ_HEALTH_DATA_IN_BACKGROUND, READ_HEALTH_DATA_HISTORY), `FOREGROUND_SERVICE_LOCATION`, `<queries>` OsmAnd
  (`net.osmand.plus`, `.osmand`, `.dev`); Service `kiesel.SpurDienst` (location); Activities
  `health.HealthConnectWritePermissionActivity`, `kiesel.KartenlinkActivity` (exportiert: Google-Maps-Links, `geo:`);
  Receiver `kiesel.GesundheitWidget` (exportiert, APPWIDGET_UPDATE), `kiesel.Wunsch`; `UpdateActionReceiver` (hat Core).
  **Gravel-Rest, der wegen der App-ID nötig ist:** Provider `${applicationId}.pebblekit`.
- **3 Core:** `permissions.PermissionRequestReceiver`.
- **4 Anker:** Rechte/queries vor `<application>`, Komponenten nach `FirmwareSideloadReceiver`. Kleinere Naht: Boulder-
  Einträge in ein eigenes Manifest (Merger), Ressourcen `@string/ks_k_titel`, `@string/ks_w_name`,
  `@xml/ks_widget_gesundheit_info` müssen dort auflösbar sein.
- **5 Risiko:** Core hat `android:authorities="coredevices.coreapp.pebblekit"` fest (Z. 19); PebbleKit-2-Clients suchen
  `packageName + ".pebblekit"` → mit `ch.dysseus.boulder` finden sie den Provider nicht.

## H Begleiter

### H1 CompanionAppLifecycleManager.kt
- **1 Pfad:** `libpebble3/src/commonMain/kotlin/io/rebble/libpebblecommon/connection/endpointmanager/CompanionAppLifecycleManager.kt`
- **2 Boulder:**
  - Konstruktor +`builtInCompanions: BuiltInCompanions`.
  - Sitzungsstart: zuerst `builtInCompanions.create(uuid, device)` → `handleBuiltInApp(uuid, app)` (eigener Scope,
    `runningApps = listOf(app)`, Nachrichten der Uhr-App an ihn) – auch für System-Apps (Kieselsport); sonst Locker.
  - `createCompanionApps`: `daneben = builtInCompanions.createBeside(id, device, pkjsRunning)` **vor** PKJSApp;
    `PKJSApp(…, bestaetigt = pkjsBestaetigt(daneben))`; `daneben` ersetzt die Plattform-Sitzung (PebbleKit Android).
  - `begleiterEingang()` = `Channel(256, DROP_OLDEST, onUndeliveredElement = Log)` statt `Channel(BUFFERED)`;
    `reicheWeiter()` loggt Fehlschläge.
  - **Nur in 0.53 an Gravel gekoppelt:** `CompanionSessionCoordinator` (`currentSessionApp`, `startSession`),
    `allowsPlatformCompanionSession` (PebbleKitSurface), `shouldRunPkjs` – in Core nicht vorhanden und nicht nötig.
- **3 Core:** `createPlatformSpecificCompanionAppControl(…, legacyPebbleKitEnabled = …)`.
- **4 Anker:** Konstruktor Z. 42–51; Channel Z. 99, Weiterleitung Z. 105–111; `createCompanionApps` Z. 121–150 (PKJSApp
  Z. 128–134, Plattform-Bedingung Z. 137); `init()` Z. 152–172: im `runningApp.onEach` nach `handleAppStop()`, **vor**
  `lockerEntryDao.getEntry(it)` (Z. 161) den Built-in-Zweig. `handleBuiltInApp`, `begleiterEingang`, `reicheWeiter` als
  `internal` in Boulder-Datei.
- **5 Risiko:** `daneben` muss Cores Bedingung `appMessageToMultipleCompanions || pkjsApp == null` ersetzen
  (`if (daneben != null) add(daneben) else if (…)`). Mit Cores Rechtemodell (F27) prüfen, wer bestätigt.
  `BegleiterEingangTest` nutzt Gravel-Typen → anpassen. DI über `initKoin` (andernorts).

### H2 PKJSApp.kt
- **1 Pfad:** `libpebble3/src/commonMain/kotlin/io/rebble/libpebblecommon/js/PKJSApp.kt`
- **2 Boulder:** Parameter `bestaetigt: Boolean = true` (neben `BestaetigtSelbst` schweigt PKJS); `replyNACK` sendet
  **NACK** statt ACK (Uhr gab die Nachricht sonst auf); Empfang je Nachricht in try/catch (`empfange()`); Sendefehler aller
  Art → `DataError` statt Sitzungsende; Konfig-URL nur `https/http/data` (`konfigurationsUrlErlaubt`). Gravel: nichts.
- **3 Core:** `ConfigMessageTarget` (`configMessages`, `onConfigMessage`); Konfig-URL zusätzlich `jsRunner.mayOpen()`.
- **4 Anker:** Konstruktor Z. 58–64; `replyNACK` Z. 80–84 (sendet weiter `ACK`); `launchIncomingAppMessageHandler` Z. 96;
  `catch (e: IllegalArgumentException)` Z. 131; Konfig-URL Z. 178–187 → `if (konfigurationsUrlErlaubt(received) &&
  jsRunner.mayOpen(received))`.
- **5 Risiko:** Cores `mayOpen` gibt für Nicht-http(s) `true` → Boulders Schemafilter bleibt nötig. NACK-Fix ändert
  Core-Verhalten (Uhr wiederholt) – gewollt.

### H3 watchModule.android.kt
- **1 Pfad:** `pebble/src/androidMain/kotlin/coredevices/pebble/watchModule.android.kt`
- **2 Boulder:** `single<BuiltInCompanions> { BoulderBegleiter(get<Context>().applicationContext) }`. Gravel: nichts.
- **3 Core:** unverändert.
- **4 Anker:** nach `singleOf(::PebbleAndroidDelegate)` Z. 15. Kleinere Naht: eigenes `boulderModule` per `includes(…)`.
- **5 Risiko:** keins.

### H4 watchModule.kt
- **1 Pfad:** `pebble/src/commonMain/kotlin/coredevices/pebble/watchModule.kt`
- **2 Boulder:** `LibPebble3.create(…, builtInCompanions = getOrNull() ?: NoBuiltInCompanions)`; `HealthViewModel` raus;
  `PebbleOsChangelogListSource` (0.3.3) raus; `FirmwareUpdateCheck(…, channel = { coreConfig.value.firmwareUpdateChannel() })`
  (0.3.2-Stand).
  **Gravel-Abhängigkeit:** DI-Block `FirmwareArtifactExpectations`, `GithubReleases`, `VerifiedFirmwareInstaller(httpClient,
  expectations, downloadDirectory = { getLockerPBWCacheDirectory(appContext) }, watchUpdateStates = …)`. Nicht:
  `networkDenyEnforcement`, `createPlatformHealthManager`, Memfault-Token, STTRouter.
- **3 Core:** `HostedPluginOAuthApi`, Bindungen `Plugins`/`Weather`, `WeatherPlugin`, `ConfigPageSessions`,
  `PermissionRequestNotifier`, `LanguagePackRepository` als single; `create` mit weiterem `get()`.
- **4 Anker:** `LibPebble3.create(` Z. 117–129 (benannt anhängen); vor `factoryOf(::Cohorts)` Z. 211 die drei
  Firmware-Singles; `singleOf(::FirmwareUpdateCheck)` Z. 212 löst `GithubReleases` selbst auf; `viewModelOf(::HealthViewModel)`
  Z. 285 darf bleiben.
- **5 Risiko:** `GithubReleases` braucht `CoreAppVersion` (Core bindet es in `androidDefaultModule` Z. 61 aus versionName
  → Mindest-App vergleicht gegen `BOULDER_VERSION`).

## I Systemapps/Locker

### I1 SystemAppIDs.kt
- **1 Pfad:** `libpebble3/src/commonMain/kotlin/io/rebble/libpebblecommon/SystemAppIDs.kt`
- **2 Boulder:** `KIESELSPORT_APP_UUID` (6c386250-…), `KIESELSTRASSE_APP_UUID` (a83bf269-…, frühere Store-App),
  `TIMESTYLE_BEDROCK_APP_UUID` (d8af11d3-…). Gravel: nichts.
- **3 Core:** unverändert.
- **4 Anker:** nach `WORKOUT_APP_UUID` Z. 16. Kleinere Naht: UUIDs in Boulder-Objekt – Core-Datei bleibt unberührt.
- **5 Risiko:** keins.

### I2 SystemApps.kt
- **1 Pfad:** `libpebble3/src/commonMain/kotlin/io/rebble/libpebblecommon/locker/SystemApps.kt`
- **2 Boulder:** Parameter `developerName = "Pebble"`; alle `defaultOrder` −1; `Workout` → `Kieselsport` (−5) +
  `Kieselstrasse` (−4); `TimeStyleBedrock` (Watchface, nur EMERY, 0, „Boulder"). Gravel: nichts.
- **3 Core:** unverändert.
- **4 Anker:** Enum Z. 7–26 (Workout Z. 21). Naht unvermeidlich (~15 Zeilen).
- **5 Risiko:** erschöpfendes `when` in Cores `LockerScreen.AppImage` (Z. 1333–1343, `Workout` Z. 1337) → I4.
  `orderIndexForInsert` (Locker.kt Z. 838, `SystemApps.entries.size`) +2.

### I3 Locker.kt
- **1 Pfad:** `libpebble3/src/commonMain/kotlin/io/rebble/libpebblecommon/locker/Locker.kt`
- **2 Boulder:** `insertSystemApps(force)` → `systemAppsEinfuegen(lockerEntryDao, force, syncLimit)` (testbar): entfernt
  Systemapps ohne Firmware-Gegenstück (Workout), macht Store-Kopien von Systemapps (Kieselsport) zu Systemapps, setzt
  Neue hinter ihren Vorgänger (`setOrder`); `SystemApps.wrap()`: `developerName = developerName`. Gravel-Änderungen
  (Rechte/Netzsperre) nicht übernehmen.
- **3 Core (7 Commits):** Plugins (pluginManifest, Plugin-PBW, verschlüsselter Speicher), Rechte (`reviewSideload/
  reviewInstall`, `recordDeclaredPermissions`), `sideloadApp(…, loadOnWatch, granted)`, `appConfigPageUrl`.
- **4 Anker:** `insertSystemApps` Z. 430–463 (Rumpf → 1 Aufruf), `SystemApps.wrap` Z. 491 (`"Pebble"` Z. 496).
  `systemAppsEinfuegen` in Boulder-Datei im Paket `locker`.
- **5 Risiko:** gering – Cores neue `LockerEntry`-Felder haben Defaults; DAO-Methoden vorhanden.

### I4 LockerScreen.kt
- **1 Pfad:** `pebble/src/commonMain/kotlin/coredevices/pebble/ui/LockerScreen.kt`
- **2 Boulder:** `SharedLockerViewModel.firmwareBuild/ohneBedrock` (`tsFirmwareBuild/tsOhneBedrock`), durchgereicht an
  `loadLockerEntries`/`loadActiveWatchface`; Karussell `karussellEintraege()` (aktives Zifferblatt bleibt drin);
  `AppImage`: `TsBedrockKachel`; Bild-`when`: `Workout` → `Kieselsport`/`Kieselstrasse`,
  `TimeStyleBedrock -> error(…)`. Gravel: nichts Nötiges.
- **3 Core:** aktive Watchapp-Sektion oben (`loadActiveWatchapp`), App-Sideload mit `reviewSideload`/`InstallReviewDialog`,
  `PendingPermissionRequests`, native Plugins, `AppType.Plugin`.
- **4 Anker:** `SharedLockerViewModel` Z. 147–172 (Z. 171), `loadLockerEntries(` Z. 381–388, `loadActiveWatchface` Z. 389,
  `myApps` Z. 450–456 (Core: `it.uuid != active?.uuid`, aktive App eigene Sektion), `AppImage` Z. 1311, Bild-`when`
  Z. 1333–1343. Kleinere Naht: `karussellEintraege` vermutlich entbehrlich (Core zeigt Aktives oben) – prüfen; ohne F26
  nur die zwei Bildzweige.
- **5 Risiko:** stark umgebaute Datei; Assets `drawable/kieselsport.png`, `kieselstrasse.png` mitnehmen.

### I5 LockerUtil.kt
- **1 Pfad:** `pebble/src/commonMain/kotlin/coredevices/pebble/ui/LockerUtil.kt`
- **2 Boulder** (fast nur TimeStyle): Parameter `firmwareBuild`, `ohneBedrock` in `loadLockerEntries`,
  `loadActiveWatchface`, `loadLockerEntry`, `LockerWrapper.load`, `asCommonApp`; `CommonAppType.System(einstellbar,
  firmwareFehlt)`; `isCompatible` zusätzlich `faehigkeit.kachel`; Helfer `firmwareHinweis()`, `aufDerUhrText()`,
  `imLockerZeigen()`, `karussellEintraege()`; `CompatibilityWarning` mit Firmware-Hinweis; Farbe. Gravel: nur Farbe.
- **3 Core:** Plugin-Typ, `loadActiveWatchapp`, Rechte-UI („undecided"), Plugin-Appstore.
- **4 Anker:** `loadLockerEntries` Z. 164 (Filter Z. 201), `loadActiveWatchface` Z. 224, `loadActiveWatchapp` Z. 232,
  `loadLockerEntry` Z. 241, `LockerWrapper.load` Z. 272, `CompatibilityWarning` Z. 322 (`coreOrange` Z. 334),
  `CommonAppType.System` Z. 461, `asCommonApp` Z. 467. Helfer in Boulder-Datei.
- **5 Risiko:** breite Naht (Parameter durch 6 Funktionen); entfällt fast ganz mit F26.

### I6 LockerAppScreen.kt
- **1 Pfad:** `pebble/src/commonMain/kotlin/coredevices/pebble/ui/LockerAppScreen.kt`
- **2 Boulder:** `loadLockerEntry(…, firmwareBuild, ohneBedrock)`; `onWatchText = entry.aufDerUhrText(…)`;
  `hasSettings()`: TimeStyle Bedrock mit `einstellbar && tsBedrockSeiteVorhanden`; `showSettings` →
  `TimeStyleBedrockSettingsRoute`. Gravel-Text „inside Gravel" gehört zu Gravels Rechte-UI (fehlt in Core).
- **3 Core:** Rechte-UI, Plugins, Konfigseite aus appinfo `configPage`.
- **4 Anker:** `loadLockerEntry` Z. 212; `onWatchText` Z. 417–430 (Core neu: `AppType.Plugin -> null`); `hasSettings`
  Z. 1001 (`System -> false` Z. 1003); `showSettings`-System Z. 1063. Kleinere Naht: nur
  `"Not Compatible with $watchName"` → `entry.firmwareHinweis() ?: "Not Compatible with $watchName"`.
- **5 Risiko:** entfällt mit F26.

## J Uhr-Einstellungen, Wetter

### J1 WatchPrefEntity.kt
- **1 Pfad:** `libpebble3/src/commonMain/kotlin/io/rebble/libpebblecommon/database/entity/WatchPrefEntity.kt`
- **2 Boulder:** `WatchPrefType.TypeBytes` (Hex-Text ↔ Bytes; kaputtes Hex → nichts senden); `hexAlsBytes()/alsHex()`;
  `enumeratePrefs` + `BytesWatchPref.entries`; `enum class BytesWatchPref` mit `TimeStyleBedrock("tsBedrock", 32 Byte
  Vorgabe)` (ein Schlüssel, weil shellpref 2048 Byte); `NumberWatchPref.BoulderMessstufe` (`boulderMessstufe`, 1..5,
  Vorgabe 3, UInt8); beim Dekodieren Messstufe ausserhalb 1..5 → Vorgabe, TS-Satz falscher Länge → verworfen.
  Gravel: nichts.
- **3 Core:** `TypeSchedule`/`ScheduleWatchPref` (Ruhezeit-Pläne), `EnumWatchPref.WindSpeed`, Sprachcodes 2–9 entfernt.
- **4 Anker:** Kodier-`when` (nach `TypeColor` Z. 125), `enum class WatchPrefType` Z. 151–161, `sealed interface WatchPref`
  Z. 221, `enumeratePrefs` Z. 233–239, `NumberWatchPref` Z. 619 (vor `BacklightTimeoutMs` Z. 630), Dekodier-`when`
  (nach `TypeColor` Z. 841), `return WatchPrefItem(` Z. 850. `BytesWatchPref`, Hex-Helfer in Boulder-Datei im selben
  Paket (sealed erlaubt das im selben Modul/Paket).
- **5 Risiko:** `sealed interface` wird erweitert → Cores erschöpfende `when`s brechen: `plugin/WatchSettingsPlugin.kt`
  (`kind()` Z. 42–48, `value()`, `setValue()`) und `WatchPrefsScreen` (J2) → **zusätzliche Naht in Core-Plugin-Code**.
  `BoulderMessstufe` erscheint automatisch als Plugin-Quelle. Ohne F26 bleibt nur die Messstufe.

### J2 WatchPrefsScreen.kt
- **1 Pfad:** `pebble/src/commonMain/kotlin/coredevices/pebble/ui/WatchPrefsScreen.kt`
- **2 Boulder:** `ohneEigeneSeiten()` (BytesWatchPref nicht in den Uhreinstellungen), `zumZuruecksetzen()` (Reset ohne
  sie); `is BytesWatchPref -> null` (`mapNotNull`); `section()`: `BoulderMessstufe -> Section.Health`,
  `TimeStyleBedrock -> Section.Other`; Messstufe als Zahlen-Eintrag mit Namen („Battery Saver" … „Precise"). Gravel: nichts.
- **3 Core:** `groupQuietTimeScheduleHours`, `schedulePref`, `WindSpeed`-Sektion.
- **4 Anker:** Mapping Z. 92–104, Reset Z. ~107–120, `section()` Z. 134, `numberPref` Z. 197 (Zweig vor `else`).
  `basicSettingsNumberItem(valueFormatter = …)` hat Core (WatchSettingsScreen Z. 2530).
- **5 Risiko:** gering; ohne F26 nur zwei Zweige für die Messstufe.

### J3 WeatherManager.kt
- **1 Pfad:** `libpebble3/src/commonMain/kotlin/io/rebble/libpebblecommon/weather/WeatherManager.kt`
- **2 Boulder:** `updateWeatherData` → `bereich.launch(start = UNDISPATCHED) { sperre.withLock { weatherAppEntryDao
  .ersetzeOrte(behalten, neue); setWeatherSettings(…) } }` mit Fehlerfang (zwei schnelle Updates verschränkten sich,
  das ältere gewann; DB-Fehler beendete die App); `wetterDerUhr()` = Ort 0 der gespeicherten Reihenfolge
  (`wetterOrtNull`, wie Firmware). Braucht `WeatherAppRealDao.ersetzeOrte` und `AppPrefsEntryDao.getWeatherSettings()`
  (Boulder, nicht [S]). Gravel: nichts.
- **3 Core:** `override val currentWeather = weatherAppEntryDao.getAllFlow()`.
- **4 Anker:** Klasse Z. 14 (`currentWeather` Z. 19, `updateWeatherData` Z. 21). `currentWeather` ersetzt
  `wetterDerUhr` nicht.
- **5 Risiko:** `wetterDerUhr` entfällt mit F26; Sperre/Atomarität bleibt als Fix.

## K Firmware

### K1 GithubReleases.kt (Gravel-Datei)
- **1 Pfad:** fehlt in Core; Boulders Fassung `pebble/src/commonMain/kotlin/coredevices/pebble/firmware/GithubReleases.kt`
  (274 Z.) mitbringen. Core fragt Core-Uhren über `EngDashOta` (dash.repebble.com).
- **2 Boulder** (gegen Gravel 0.3.2, das Boulder behält; 0.3.3 = Changelog-Liste/Picker, verworfen):
  `RELEASES_URL = …/repos/dysseus-pascal/boulder-firmware/releases`; +`appFassung: CoreAppVersion`; zuerst nach
  `MindestApp.erfuellt` filtern, dann `selectRelease`; `BrauchtNeuereApp`, wenn nur eine neuere auf die App wartet;
  `.sig`-Asset mit 64 Byte Pflicht; `ExpectedFirmwareArtifact.signatur = Signaturquelle.Signiert(url, name)`;
  ETag/304-Cache; DTO `body`.
  Gravel-Anteil (0.3.2): Auswahl (`selectRelease`, `ReleaseTagVersion`, Kanal), sha256/Grösse aus API-`digest`.
- **3 Core:** –
- **4 Anker:** Aufruf aus `FirmwareUpdateCheck.doCheck` (K3), DI in watchModule (H4).
- **5 Risiko:** GitHub-Limit 60/h je IP (auch 304); Kanal für Boulder-Releases wirkungslos → fest `Soaked`.

### K2 VerifiedFirmwareInstaller.kt (Gravel-Datei)
- **1 Pfad:** fehlt in Core; `pebble/src/commonMain/kotlin/coredevices/pebble/firmware/VerifiedFirmwareInstaller.kt`
  (560 Z.) mitbringen.
- **2 Boulder** (gegen 0.3.2): `signaturSchluessel = FirmwareSignatur.schluessel()`; Signatur-URL nur https; Signatur
  (genau 64 Byte, `ladeSignatur`) vor dem Download; nach sha256 `FirmwareSignatur.pruefe(aussage(tag, dateiname, groesse,
  sha256), …)`; signiertes Paket ohne Manifest-Tag → abgelehnt.
  Gravel-Anteil: Download mit Grössendeckel, sha256 (okio `HashingSink`), Stall-Timeout, PBZ-Manifest/CRC,
  Übergabe per `device.sideloadFirmware(path)`, `ForkFirmwareInstallState`.
- **3 Core:** –
- **4 Anker:** Core installiert an **vier** Stellen ungeprüft über `updateFirmware(FoundUpdate)` (Download in libpebble3):
  `WatchesScreen` Z. 2179, `WatchOnboardingScreen` Z. 201 (composeApp), `FirmwareUpdateUiTracker.updateWatchNow` Z. 103,
  `DebugFirmwareSideload` Z. 231 → je auf `installer.install(device, update)` (Gravel-Haken). Alternative mit einer Naht:
  Prüf-Schnittstelle in libpebble3 `FirmwareUpdater.updateFirmware`, die der Installer belegt.
- **5 Risiko:** fehlt ein Haken, läuft Firmware ohne Ed25519-Prüfung auf die Uhr. Braucht `okio` in pebble/commonMain,
  `ForkFirmwareInstallUi.kt`, Boulders `FirmwareSignatur.kt`/`Ed25519.*`. libpebble3-APIs (`FirmwareUpdateStatus`,
  `PbzFirmware`, `sideloadFirmware`) in Core unverändert.

### K3 FirmwareUpdateCheck.kt
- **1 Pfad:** `pebble/src/commonMain/kotlin/coredevices/pebble/firmware/FirmwareUpdateCheck.kt`
- **2 Boulder** (gegen 0.3.3): `CACHE_TTL` 15 min → **5 h** (Nutzerentscheid; unter 6 h wegen 6-h-Lauf). Rest ist Gravel
  0.3.2: Kanal-Parameter, `CacheKey.channel`, Route-Enum (Memfault/GithubReleases/Cohorts), EngDash-Vorrang über
  `BUG_URL` + `useEngDashOta`. Boulder braucht davon nur: Core-Uhren → `githubReleases.getLatestFirmware(…)`.
- **3 Core (333877b8):** `doCheck` = UNKNOWN → Failed, `isCoreDevice()` → `engDashOta`, sonst `cohorts`;
  `useEngDashOta` und `Memfault.getLatestFirmware` entfernt.
- **4 Anker:** Konstruktor Z. 42–49 (+`githubReleases`), `doCheck` Z. 98–102:
  `isCoreDevice() -> githubReleases.getLatestFirmware(watch, FirmwareUpdateChannel.Soaked)`, `CACHE_TTL` Z. 105.
  ≈ 4 Zeilen; Gravels 0.3.2-Fassung kompiliert auf Core nicht.
- **5 Risiko:** ohne Haken bietet Core der Uhr Core-Firmware an (Abnahme 5). `FirmwareUpdateCheckRoutingTest`/
  `FirmwareSparTest` anpassen.

## L Firmware-Benachrichtigung

### L1 FirmwareUpdateUiTracker.android.kt (+ common)
- **1 Pfad:** `pebble/src/androidMain/kotlin/coredevices/pebble/firmware/FirmwareUpdateUiTracker.android.kt`;
  `pebble/src/commonMain/kotlin/coredevices/pebble/firmware/FirmwareUpdateUiTracker.kt`
- **2 Boulder:** Android-Datei = Core-Fassung (Gravels 0.3.3-Picker-Extras zurück, „Update Now"-Aktionen und
  `UpdateActionReceiver` wieder da), byte-gleich mit 96eb261a und ec2156cc. Common: Gravel-0.3.2-Haken behalten
  (`installer: VerifiedFirmwareInstaller`, `installer.install(device, update)` statt `updater.updateFirmware(update)`).
- **3 Core:** beide unverändert.
- **4 Anker:** Android: **keine Naht**. Common (Gravel-Abhängigkeit): Konstruktor Z. 24–29, `updateWatchNow` Z. 84–104
  (Z. 103).
- **5 Risiko:** ohne Common-Haken umgeht „Update Now" (Benachrichtigung, Uhr, Deep-Link) die Signatur.

## M Fixes

### M1 PebbleDeepLinkHandler.kt (+ .android.kt)
- **1 Pfad:** `pebble/src/commonMain/kotlin/coredevices/pebble/PebbleDeepLinkHandler.kt`;
  `pebble/src/androidMain/kotlin/coredevices/pebble/PebbleDeepLinkHandler.android.kt`
- **2 Boulder:** `enum SideloadArt(endung) { Firmware, App, Sprachpaket }`, `PendingFirmwareSideload(…, art)`;
  `behandleSideload()` für `.pbz/.pbw/.pbl`: Kopie im Hintergrund (`Dispatchers.IO`, `sideloadSperre`), dann Rückfrage
  (MainActivity nimmt Dateien jeder App an); `confirmPendingFirmwareSideload` je Art; `reserveSideloadCopy(…, art)`.
  „Update Now"-Pfad (`handleShowWatches(path)`, `updateNowUri`) = Core-Stand (Gravel 0.3.3 hatte ihn entfernt).
  Android: `writeFile` mit Deckel 32 MB und gefangener Ausnahme.
- **3 Core:** `.pbw` → `pendingAppSideload` → LockerScreen Z. 274–291 (`reviewSideload`, sonst **sofort** installieren);
  Deep-Link `permission-requests`. Android-Datei unverändert.
- **4 Anker:** `PendingFirmwareSideload` Z. 68, Endungs-`when` Z. 157–159 und Z. 174–176, `handleLanguagePack` Z. 181,
  `handleFirmware` Z. 201, `reserveSideloadCopy` Z. 218 (`.pbz` fest Z. 223), `confirmPendingFirmwareSideload` Z. 244,
  `handleApp` Z. 281; `handleShowWatches(path)` Z. 352 schon da → keine Naht. Android `writeFile` Z. ~43–62.
  Kleinere Naht: `.pbw` über Cores `pendingAppSideload` und dort Rückfrage auch bei `review == null`; `SideloadArt`
  nur für pbz/pbl.
- **5 Risiko:** Boulders App-Zweig `libPebble.sideloadApp(file)` ohne `granted` umgeht Cores Rechteprüfung (F27).

## N Effizienz

### N1 CommonAppDelegate.kt
- **1 Pfad:** `composeApp/src/commonMain/kotlin/coredevices/coreapp/CommonAppDelegate.kt`
- **2 Boulder:** Wetter und `performBackgroundWork` nur mit verbundener Uhr; eigene Zeitstempel
  `boulder_letztes_wetter_ms` / `boulder_letzte_hintergrundarbeit_ms` unter `nachholSperre`; `performBackgroundWork` an
  `arbeit` statt `doFullSync`; beim Verbinden `nachholenBeimVerbinden(…) { holeNach() }` (nur Fälliges, eigener Job,
  Fehler gefangen); Helfer `faellig()`. Gravel (whisper-Migration, Firebase-Reste) nicht übernehmen.
- **3 Core:** `SyncManager.resume()` (Index) nach `startAutoSync`; STT-Versionsschlüssel.
- **4 Anker:** Klasse Z. 49–72, `startAutoSync` Z. 176 (danach Nachholen starten), `doBackgroundSync` Z. 185–240
  (Jobs Z. 213–236). Helfer in Boulder-Datei (`NachholenTest` existiert).
- **5 Risiko:** gering; `processHeartbeat()` (Mixpanel) bleibt an `doFullSync` (F25).

## O Build

### O1 pebble/build.gradle.kts
- **1 Pfad:** `pebble/build.gradle.kts`
- **2 Boulder:** `withHostTestBuilder {}.configure { isIncludeAndroidResources = true }`; androidMain:
  `libs.health.connect` (connect-client 1.1.0, SpO2), `libs.osmdroid`, `project(":osmand-api")`,
  `libs.androidx.documentfile`, `libs.androidx.work`; `androidHostTest`: `libs.robolectric`.
  **Gravel-Abhängigkeit:** `implementation(libs.okio)` (commonMain, Installer); commonTest `libs.ktor.client.mock`,
  `libs.coroutines.test` (Firmware-Tests mit MockEngine). Nicht: firebase-stubs, whisper, health-kmp-GMS-Ausschluss (F25).
- **3 Core:** unverändert.
- **4 Anker:** `withHostTestBuilder {}`, `androidMain { dependencies { … } }`, danach `getByName("androidHostTest")`,
  `commonMain`/`commonTest`. Katalog: `okio`, `ktor-client-mock`, `coroutines-test`, `androidx-work`,
  `androidx-documentfile` hat Core; **`health-connect`, `osmdroid`, `robolectric` fehlen** in `libs.versions.toml`.
- **5 Risiko:** gering.

---

## Gravel-Dateien, die Boulders Code braucht (in Core 1.15 nicht vorhanden)

Ermittelt über Importe/Paketnutzung aller neuen Boulder-Dateien und der Boulder-Zeilen in geänderten Dateien
(Skript, nachgeprüft). Zeilen = Fassung aecd3350.

| Datei | Zeilen | Wer braucht sie |
|---|---|---|
| `pebble/src/commonMain/kotlin/coredevices/pebble/firmware/GithubReleases.kt` | 274 | FirmwareUpdateCheck; Tests |
| `…/pebble/firmware/VerifiedFirmwareInstaller.kt` | 560 | WatchesScreen, FirmwareUpdateUiTracker, WatchOnboardingScreen, DebugFirmwareSideload |
| `…/pebble/firmware/FirmwareArtifactExpectations.kt` | 75 | GithubReleases, Installer, Boulders `FirmwareSignatur.kt`, `Ed25519.android/ios.kt` (`Signaturquelle`, `ExpectedFirmwareArtifact`) |
| `…/pebble/firmware/FirmwareReleaseSelection.kt` | 166 | GithubReleases (`selectRelease`, `SelectableRelease`), `FirmwareSignatur.kt` und `timestyle/TsBedrockFaehigkeit.kt` (`ReleaseTagVersion`, `.boulder`), watchModule (`firmwareUpdateChannel()`) |
| `pebble/src/commonMain/kotlin/coredevices/pebble/ui/ForkFirmwareInstallUi.kt` | 46 | WatchesScreen, WatchOnboardingScreen (`installStateFor`, `stateTextSuffix`, `ForkInstallProgressBar`) |
| **Summe Pflicht** | **1 121** | dazu Gravel-Haken in 4 Core-Dateien (K2), DI-Block (H4), `okio` (O1) |

Nicht nötig (nur durch Gravel-Teile der [S]-Dateien oder Tests erreicht):
- `util/src/commonMain/kotlin/coredevices/util/ForkLinks.kt` (36) – nur Gravels `KnownIssuesLink`,
  `WatchappPermissionsScreen`, `OnboardingScreen`; Boulder änderte nur URL/Tag-Format.
- `libpebble3/…/endpointmanager/CompanionSessionCoordinator.kt` (199), `libpebble3/…/pebblekit/PebbleKitSurface.kt` (76) –
  nur in 0.53 Aufhänger für Boulders Begleiter-Zeilen; in Core direkt verankern (H1).
- `libpebble3/…/com/anopticlabs/gravel/pkjs/NetworkDenyEnforcement.kt` (39), `libpebble3/…/locker/WatchappPermissions.kt`
  (123) – nur `BegleiterEingangTest`.
- Whisper/SelfHostedServer/WhatsNew/WatchappPermissionsScreen/ForkDictationDebugItems – nur aus Gravels Teilen von
  WatchSettingsScreen/PebbleRoutes/LockerAppScreen.

Tests Gravel-Ursprungs zum Firmware-Pfad (von Boulder geändert, fehlen in Core): `VerifiedFirmwareInstallerTest` (777),
`GithubReleasesTest` (475), `FirmwareUpdateCheckRoutingTest` (302), `FirmwareReleaseSelectionTest` (268),
`TestFirmwareFixtures` (101), `FirmwareArtifactExpectationsTest` (68), `TestWatches` (59), `CohortsTest` (43).
Boulder-eigen: `MindestApp.kt` (55), `FirmwareSignatur.kt` (57), `Ed25519.{android,ios}.kt`.
Nicht-[S]-Begleitdateien mit Boulder-Zeilen, an denen Haken hängen: `FakeLibPebble.kt`, `HealthDao.kt`,
`HealthDaoExt.kt`, `Housekeeping.kt`, `HealthSyncTracker.kt`, `WeatherAppRealDao.kt`, `AppPrefs.kt`, `CoreConfig.kt`
(`firmwareUpdatesEarlyChannel`, entfällt mit Querschnitt 5), `MyCollectionScreen.kt` (TS), `WatchOnboardingScreen.kt`
und `DebugFirmwareSideload.kt` (Installer-Haken).
