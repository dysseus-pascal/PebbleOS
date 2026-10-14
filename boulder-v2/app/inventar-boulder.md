# Inventar: Boulder 0.53.0 (aecd3350) gegenüber Gravel 0.3.3

Bestandsaufnahme vom 14.10.2026 (Rechercheauftrag, nachgelesen). Grundlage: die Git-verfolgten Dateien
beider Bäume; `.kotlin/`, `local.properties`, `*/.cxx/` und whisper.cpp sind nicht mitgezählt.

## 0. Gesamtzahlen

- **673 neue** Dateien (davon 206 in `osmand-api/` und 72 Release-Notizen), **110 geänderte** (+4 769/−2 391 Zeilen) und **57 entfernte** Dateien.
- Neu: 156 Kotlin-Produktivdateien (29 081 Zeilen), 144 Testdateien (21 049), 7 Schema-JSON (12 158), 58 Binärdateien.
- Gravel 0.3.4 wurde in 36fad7e2 übernommen und in aecd3350 wieder entfernt; HEAD enthält davon nichts.
- 8 der 110 geänderten Dateien sind byte-gleich mit Gravel 0.3.2: Boulder hat Gravels Firmware-Auswahl aus 0.3.3 (Changelog-Liste, Build-Picker) nicht übernommen und die Kanal-Logik aus 0.3.2 behalten.

## 1. Teile von Boulder

Abkürzungen: `PA/` = `pebble/src/androidMain/kotlin/coredevices/pebble/`, `P/` = `pebble/src/commonMain/kotlin/coredevices/pebble/`, `L/` = `libpebble3/src/commonMain/kotlin/io/rebble/libpebblecommon/`, `K/` = `PA/kiesel/`.

| Teil | Zweck | Eigene Dateien | Umfang |
|---|---|---|---|
| Health (Kern von Kiesel-Helper) | Reiter „Health“: Tagesband, Heart & Body, Nachtansicht, Goals; liest libpebble3 und Health Connect („Akte“) | 37 Dateien in `K/`; `P/ui/GesundheitScreens.kt` mit android/ios | 7 638 + 261 Zeilen |
| Trends / „What makes a difference“ | Tagesreihen und Gruppenvergleiche | 7 Dateien in `K/` | 1 960 Zeilen |
| Akku | Akkuseite aus den Nächten | `K/Akkurechnung.kt`, `K/ansicht/AkkuAnsicht.kt` | 252 Zeilen |
| Widget | Homescreen-Widget | `K/GesundheitWidget.kt`, `K/Widgetlage.kt` | 685 Zeilen |
| Sicherung / Tagesarchiv | tägliche JSON-Sicherung (SAF, WorkManager), Tagesarchiv über die 90-Tage-Löschung hinaus | `K/Sicherung.kt`, `K/Tagesspeicher.kt`, `K/ansicht/SicherungAnsicht.kt` | 772 Zeilen |
| Kieselsport (Training) | eingebauter Begleiter der Systemapp; Trainings nach Health Connect, GPS-Spur, osmdroid-Karten | 24 Dateien in `K/`; `P/ui/TrainingScreens.kt` mit android/ios | 6 995 + 130 Zeilen |
| Drinktervall / SupCycle | `Mithoerer` neben deren PKJS: Wasser, Kaffee, Präparate nach Health Connect | 6 Dateien in `K/` | 1 623 Zeilen |
| Kieselstrasse (OsmAnd) | OsmAnds nächste Abzweigung (AIDL) auf die Systemapp; Kartenlinks an OsmAnd | 5 Dateien in `K/`; Modul `osmand-api/` (GPLv3, unverändert) | 1 256 Zeilen; Modul 8 694 Zeilen |
| Kiesel-Ressourcen | Strings, Widget-Layout, Farben | `pebble/src/androidMain/res/**` ohne TimeStyle | 19 Dateien |
| Einstellungs-Brücke | `boulderSettingsItems`, `boulderAbzeichen` | `P/ui/BoulderSettings.kt` mit android/ios | 294 Zeilen |
| BuiltInCompanions | libpebble3-Schnittstelle für eingebaute Begleiter | `L/connection/BuiltInCompanions.kt` | 47 Zeilen |
| Nächte aus Firmware-DLS | Tags 88/89 und 0x4001/0x4002, Quarantäne für Unbekanntes aus 0x4000–0x40FF; Nächte in `boulder.db` | `L/datalogging/BoulderDls.kt`, Parser, Entity, DAO, `L/database/boulder/*` | 12 Dateien, 1 076 Zeilen |
| Datalogging-Robustheit | erst sichern (fsync), dann ACK; Quarantäne ≤ 20 MB | `L/datalogging/{DlsQuarantaene,Schubsammler,Dauerhaft}.kt`, `L/util/Datenbereich.kt` | 7 Dateien, 349 Zeilen |
| SpO2 | Minutendaten Fassung 14, `spo2_readings`, drei Uhr-Einstellungen, HC-Export | Entity, DAO, `OxygenSaturationPlatform` | 5 Dateien, 139 Zeilen |
| Health-Connect-Zusätze | Nächte mit Phasen, SpO2, Tagesblöcke, alle Rechte auf einmal, Abruf beim Öffnen | `P/health/*`, `PA/health/HealthConnectPermissions.android.kt` | 9 Dateien, 594 Zeilen |
| Goals | `boulder_ziele` über die HealthStats-BlobDB an V2 | `L/health/BoulderZiele.kt`; UI in `K/` | 41 Zeilen |
| TimeStyle Bedrock | Einstellungen und Vorschau des Zifferblatts der Boulder-Firmware (b22–b26, auf V2 ausgeblendet) | `P/timestyle/**` u. a.; 27 Ressourcen | 23 Dateien, 4 905 Zeilen |
| Firmware-Update | Releases aus `boulder-firmware`, Ed25519-Signatur, Mindest-App, V2-Erkennung | `P/firmware/FirmwareSignatur.kt`, `MindestApp.kt`, `Ed25519.*` | 271 Zeilen (dazu Gravels Firmware-Pfad) |
| Downgrade-Schutz / DB-Fassungen | in `L/database/Database.kt` | Schema-JSON 46–50 und 1045 | – |
| Branding, CI/Release | Logo, Symbol, `BOULDER_VERSION`, `ci/debug.keystore`, `.github/release/*.md` | | |

## 2. Geänderte Gravel-Dateien (110), nach Teil

**[S]** = Haken in Upstream- oder Gravel-Code, auf Core neu einzusetzen (45 Dateien).

- **A Nächte/Datalogging/Minutendaten** (9, +744/−342): `Datalogging.kt` [S], `HealthDataProcessor.kt` [S], `DataLoggingService.kt` [S], `HealthDataParser.kt` [S], `HealthDaoExt.kt`, `HealthRecords.kt` [S], `HealthDataEntity.kt` [S], 2 Tests.
- **B Datenbank, Downgrade, Backup** (5, +234/−6): `Database.kt` [S], `LibPebbleModule.kt` [S], `backup_rules.xml` [S], `full_backup_content.xml` [S], Test.
- **C HealthDataApi** (4, +252/−165): `LibPebble.kt` [S], `Health.kt` [S], `FakeLibPebble.kt`, `HealthDao.kt`.
- **D SpO2** (4, +217/−17): `Housekeeping.kt`, `BlobDB.kt` [S], `HealthSettingsRealDao.kt` [S], `HealthSettings.kt` [S].
- **E Einstellungen** (4, +111/−139): `WatchSettingsScreen.kt` [S] (Sektionen, `boulderSettingsItems`, SpO2, Early-Kanal), android/ios-Teile, `CoreConfig.kt`.
- **F Health-Connect-Export** (3, +232/−47): `PlatformHealthSync.kt` [S], `HealthSyncTracker.kt`, Test.
- **G Navigation, App-Start, Manifest** (6, +355/−72): `PebbleRoutes.kt` [S], `WatchHomeScreen.kt` [S], `WatchesScreen.kt` [S], `PebbleAndroidDelegate.kt` [S], `PebbleAppDelegate.kt` [S], `AndroidManifest.xml` [S].
- **H Begleiter** (4, +170/−39): `CompanionAppLifecycleManager.kt` [S], `PKJSApp.kt` [S], `watchModule.android.kt` [S], `watchModule.kt` [S].
- **I Systemapps/Locker** (7, +220/−88): `SystemAppIDs.kt` [S], `SystemApps.kt` [S], `Locker.kt` [S], `LockerScreen.kt` [S], `LockerUtil.kt` [S], `LockerAppScreen.kt` [S], `MyCollectionScreen.kt`.
- **J TimeStyle, Messstufe, Wetter** (5, +196/−20): `WatchPrefEntity.kt` [S], `WatchPrefsScreen.kt` [S], `WeatherManager.kt` [S], `WeatherAppRealDao.kt`, `AppPrefs.kt`.
- **K Firmware** (12, +1249/−574): `GithubReleases.kt` [S], `VerifiedFirmwareInstaller.kt` [S], `FirmwareArtifactExpectations.kt`, `FirmwareReleaseSelection.kt`, `FirmwareUpdateCheck.kt` [S], `network_security_config.xml`, 5 Tests, `KNOWN_ISSUES.md`.
- **L Gravels Firmware-Picker nicht übernommen** (9, +113/−139): u. a. `FirmwareUpdateUiTracker.android.kt` [S].
- **M Fixes** (6, +163/−66): Sideload-Rückfrage, AppMessage-NACK, Timeline-DELETE, https-Konfigseiten; `PebbleDeepLinkHandler.kt` [S].
- **N Effizienz** (4, +175/−21): `CommonAppDelegate.kt` [S] (Arbeit nur mit Uhr), `MainApplication.kt`, `logging.kt`, `CoreAnalytics.kt`.
- **O Build/CI** (8, +134/−171): `build.yml`, `androidApp/build.gradle.kts` [S], `proguard-rules.pro`, `libs.versions.toml`, `libpebble3/build.gradle.kts`, `pebble/build.gradle.kts` [S], `settings.gradle.kts` [S], Test.
- **R Rebrand Gravel→Boulder, Doku** (20, +204/−485).

## 3. Entfernte Gravel-Dateien (57)

| Gruppe | Grund |
|---|---|
| Gravels Firmware-Auswahl aus 0.3.3 (16) | hätte Core-Builds angeboten und die Boulder-Firmware ersetzt |
| F-Droid (26) | kein F-Droid in Boulder |
| Gravels Health-UI (8) | durch Kiesel-Helper ersetzt; Workout durch Kieselsport |
| Branding (7) | Boulder-Logo |

## 4. Datenbanken

- **`libpebble3.db`**: Fassung `UPSTREAM_VERSATZ (1000) + 45 = 1045`. Boulder-eigen: Tabelle `spo2_readings`, Spalte `overlay_data.manual`; Zeilen `boulder_ziele`, SpO2-Einstellungen, WatchPrefs `tsBedrock`, `boulderMessstufe`; das Feld `distanceM` liegt auf der Spalte `distanceCm` (Meter). Dateien aus 0.48–0.50 enthalten noch die alten Tabellen `boulder_nacht*` (Room kennt sie nicht mehr, `NachtUmzug` liest sie).
- **Core** steht bei **49**: 46 `CalendarEntity.writable`; 47 `LockerEntryEntity.configPage`, `pluginManifest`; 48 Tabelle `AppPermission` statt `LockerAppPermission`, `LockerEntryEntity.manifestVersion` (Spec `AutoMigration48`); 49 `AppPermission.seenAtMs`. Upstream 45 hat in Boulder und Core denselben identityHash.
- **`boulder.db`** (`BoulderDatenbank`, Fassung 1): `boulder_nacht`, `boulder_nacht_abschnitt`, `boulder_nacht_hrv`, `boulder_umzug`.
- **`DowngradeSchutz`**: verschiebt eine Datei mit höherer `user_version` nach `databases/beiseite/`, statt sie leeren zu lassen.

## 5. Tests

144 Testdateien (985 `@Test`), davon 85 in `pebble/src/androidHostTest` (Robolectric), 18 Room-Tests in `libpebble3/src/jvmTest` (`SchemaWaechterTest`, `DatenbankFassungTest`, `NachtUmzugTest`). Gesamtlauf von 0.53 mit Gravels Tests: 2 143.

## 6. Fakten für den Neuaufbau (im Core-Klon nachgesehen)

- Boulders Firmware-Pfad baut auf Gravel-Dateien, die Core nicht hat (`GithubReleases`, `VerifiedFirmwareInstaller`, `FirmwareArtifactExpectations`, `FirmwareReleaseSelection`, `ForkLinks`).
- Begleiter: Boulders Haken sitzt in `CompanionAppLifecycleManager.startSession`.
- Datalogging: Core geht noch den alten Weg (`logData(…, itemsLeft)`, `handleSendDataItems`, `HEALTH_TAGS`); Gruppe A ist neu einzusetzen.
- Vorhanden in Core: `SystemApps.Workout`, `Section`/`rawSettingsItems` in `WatchSettingsScreen`.
- Zustand ausserhalb der Datenbanken (bleibt bei gleicher App-ID): SharedPreferences `kiesel-*`, `boulder-einmalig`, `boulder-ziele`, `boulder_rechtefrage`; Ordner `kieselsport-wartend`, `puls`, `pausen`, `spuren`, `dls-quarantaene`, `databases/beiseite`. Trainings, Wasser und Präparate liegen in Health Connect.
