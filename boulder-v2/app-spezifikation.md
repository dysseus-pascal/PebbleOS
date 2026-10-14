# Schritt „App-Teile“: Boulder auf der App von Core Devices (Entwurf, 14.10.2026)

Grundlage: `entscheidungen.md` („Grundlage“ 05.10., „Boulder-App“ 13./14.10., F19 A). Boulder baut
nicht mehr auf Gravel, das selbst ein Fork von Core ist, sondern direkt auf
[coredevices/mobileapp](https://github.com/coredevices/mobileapp). App-ID `ch.dysseus.boulder` und
Signatur bleiben; die Daten von 0.53 ziehen beim Update mit. Belege: `app/inventar-boulder.md`
(Boulder gegenüber Gravel 0.3.3), `app/core-seit-gravel.md` (Core seit Gravels Basis).

## 1. Ausgangslage

| | |
|---|---|
| Boulder heute | 0.53 (`aecd3350`) = Gravel 0.3.3 + Boulders Teile |
| Gravels Core-Stand | `96eb261a` (26.08., etwa Core 1.10) |
| Core heute | 1.15.0.1 (`ec2156cc`, 08.10.): 150 Commits mehr, Datenbank Fassung 49 |
| Boulders Teile | 673 neue Dateien: 156 Kotlin-Dateien (29 081 Zeilen), 144 Testdateien (21 049 Zeilen, 985 Tests), Modul `osmand-api` (206 Dateien) |
| Haken in fremdem Code | 110 geänderte Gravel-Dateien, davon 45 Haken in Core- oder Gravel-Code |
| Gravel-Code, den Boulder braucht | Firmware-Pfad (`GithubReleases`, `VerifiedFirmwareInstaller`, `FirmwareArtifactExpectations`, `FirmwareReleaseSelection`) und `ForkLinks`; Core hat ihn nicht |

Was Core seither geändert hat und Boulder trifft:
- **Datalogging umgebaut** (2a625912): unbekannte System-Tags werden bestätigt und verworfen. Ohne
  Boulders Zweig gingen die Nächte der V2 (0x4001/0x4002) verloren. Der ACK geht weiter vor dem
  Sichern raus.
- **Firmware:** Core-Uhren fragen `dash.repebble.com` (333877b8). Boulders GitHub-Weg muss neu hinein.
- **PKJS und Plugins:** Laufzeit umgebaut, neues Rechtemodell (Tabelle `AppPermission`, Fassung 48
  löscht Gravels `LockerAppPermission`). Ohne eingeschaltete Plugins bekommt jedes PKJS Netz und
  Standort.
- **Einstellungen:** `WatchSettingsScreen` von 7 Commits geändert; Uhr-Einstellungen mit neuen Typen.
- **Ring/Index** (rund die Hälfte der Änderungen) betrifft Boulder nicht.

## 2. Vorgehen

### 2.1 Repo: Core-Verlauf statt Fork vom Fork
- Ein Zweig im Boulder-Repo beginnt bei Core 1.15.0.1, mit Cores ganzem Verlauf.
- Boulders Teile kommen als Commits darauf.
- Zum Schluss ein Merge der alten Boulder-Geschichte mit der Strategie `ours`: Der Baum bleibt der
  neue, die Geschichte bis 0.53 bleibt erhalten. `main` zieht per Fast-Forward nach, ohne Force-Push.
- Spätere Core-Fassungen kommen per Merge eines Core-Tags, wie bei der Firmware. Bis zum Gerätetest
  der neuen App bleibt Core eingefroren.
- `BOULDER_VERSION` und `versionCode` = Zahl der Commits bleiben (Cores Schema bricht bei einem Tag
  wie `v0.54.0`). Mit Cores Verlauf liegt die Zahl über der installierten 0.53.

### 2.2 Aufteilung: eigene Ordner, wenige Nähte
- Wie bei der Firmware V2: Boulders Code liegt in eigenen Ordnern und Modulen (Kiesel-Teile, Nächte und
  `boulder.db`, Firmware-Pfad, `osmand-api`). Core-Code wird nur an erlaubten Stellen geändert, jede mit
  „Boulder:“ markiert.
- Ein App-Wächter (wie `k1_waechter.py`) prüft den Diff gegen den Core-Tag: nur die Stellen der Liste,
  mit Höchstwerten.
- Die Nähte (heute 45 Dateien) werden dabei weniger, wo Core inzwischen eine Schnittstelle hat:
  - Datalogging: Boulders Tags und das „erst sichern, dann ACK“ an Cores neue Aufteilung
    (System/Drittanbieter).
  - DI: eigene Koin-Module statt Änderungen in Cores Modulen, wo möglich.
  - Einstellungen und Navigation: Boulders Sektionen und Routen über die Haken, die Core hat
    (`Section`, `rawSettingsItems`).
  - Systemapps/Locker: Kieselsport und Kieselstrasse statt Workout.

### 2.3 Datenumzug von 0.53
- Gleiche App-ID und Signatur: SharedPreferences (`kiesel-*`, `boulder-*`), Dateien in `filesDir`
  (`kieselsport-wartend`, `spuren`, `dls-quarantaene` …) und `boulder.db` bleiben unverändert stehen.
- `libpebble3.db` steht bei **1045** (1000 + Upstream 45, dazu `spo2_readings` und
  `overlay_data.manual`). Core steht bei 49. Die neue App zählt weiter mit Versatz: **1049**; Cores
  Migrationen 46–49 laufen als 1045→1046→…→1049. Mit 49 sähe Room eine Herabstufung und leerte
  alles.
- 1047→1048 löscht `LockerAppPermission` (Gravels PKJS-Freigaben). Was daraus wird, hängt an F27.
- `DowngradeSchutz` und `NachtUmzug` bleiben.
- Rückweg auf 0.53: Der `DowngradeSchutz` von 0.53 legt die neuere Datenbank beiseite, 0.53 startet
  leer; die Daten liegen in `databases/beiseite/`.

### 2.4 Was nicht mitkommt (Vorschlag)
- Gravels eigene Funktionen ausser dem Firmware-Pfad: F-Droid-Wächter, UDP-Filter, PebbleKit-Schalter,
  Prüf-Tasks (je nach F25 und F27).
- TimeStyle Bedrock (4 905 Zeilen), falls nur noch V2 unterstützt wird (F26).

## 3. Arbeitspakete (Vorschlag)

| AP | Inhalt |
|---|---|
| AP0 | Core 1.15.0.1 als Zweig im Boulder-Repo; Bau mit `ch.dysseus.boulder`, Signatur, `BOULDER_VERSION`; Google nach F25; CI |
| AP1 | Datenbank 1045→1049, `SchemaWaechterTest` gegen Upstream 49, `DowngradeSchutz`, `boulder.db`; Migrationstest mit Daten in jeder Tabelle |
| AP2 | Datalogging und Health-Daten auf Cores neuem Weg: `BoulderDls`, Minutendaten v14 mit SpO2, Quarantäne, ACK nach dem Sichern |
| AP3 | Firmware-Pfad: GitHub-Releases von `boulder-firmware`, Ed25519, Mindest-App, V2; Cores OTA für Boulders Uhr aus |
| AP4 | Begleiter und Systemapps: `BuiltInCompanions` (Kieselsport, Drinktervall, SupCycle), Kieselstrasse, Locker |
| AP5 | Kiesel-Teile: Health, Trends, Akku, Widget, Sicherung, Training, Wasser/Stack, Kieselstrasse/OsmAnd; Einstellungen und Navigation |
| AP6 | Health Connect: Export und Rechte |
| AP7 | Branding, Release-Notizen, App-Wächter |
| AP8 | Prüfung und Abnahme, Vorabversion |

## 4. Abnahmekriterien (Entwurf)

1. **Basis:** Boulders `main` enthält Core 1.15.0.1 mit Verlauf. Core-Code ist nur an den Nähten der
   Liste geändert; der App-Wächter ist grün.
2. **Update ohne Verlust:** Die neue App installiert sich über 0.53 (gleiche App-ID und Signatur,
   höherer versionCode). `libpebble3.db` geht von 1045 auf 1049 ohne Zeilenverlust (Test mit Daten in
   jeder Tabelle); `boulder.db`, Einstellungen und Dateien bleiben.
3. **Daten der Uhr:** Nächte (0x4001/0x4002), Minutendaten v14 mit SpO2 und Sitzungen kommen von der
   V2 an. Nichts wird bestätigt, bevor es gesichert ist; Unbekanntes geht in die Quarantäne.
4. **Boulders Teile wie in 0.53:** Health, Trends, Widget, Sicherung, Goals, Training, Wasser/Stack,
   Kieselstrasse und Health Connect. Alle übernommenen Boulder-Tests sind grün.
5. **Firmware:** Updates kommen nur aus `boulder-firmware`, signiert, mit Mindest-App. Die Uhr bekommt
   nie Cores Firmware angeboten.
6. **Core bleibt Core:** Cores eigene Tests sind grün; Benachrichtigungen, PKJS, Locker, Wetter und
   Kalender laufen.
7. **Netz:** Die Netzwerkziele der App sind aufgelistet und entsprechen F25; ohne Einwilligung geht
   nichts an Google oder Mixpanel.
8. **Release und Gerät:** Der Release-Build ist mit dem CI-Schlüssel signiert, lässt sich über 0.53
   installieren, und eine Nacht der Uhr kommt an.

## 5. Entscheide (14.10.2026, `entscheidungen.md`)

- **F24** Basis Core 1.15.0.1, eingefroren bis zum Gerätetest der App (Vorschlag).
- **F25: A** Google-Teile drin, stillgelegt (Attrappe, keine Tokens, Sammeln aus).
- **F26: A** nur Firmware V2; TimeStyle Bedrock entfällt.
- **F27: A** PKJS-Rechte nach Cores Modell.
- **F28:** Cactus bleibt.

## 6. Stand der Umsetzung

### AP0 (14.10.2026)
- **Grundlage:** Merge-Commit `3256eabfc` im Boulder-Zweig `claude/exciting-cerf-m8apu2`. Erster
  Elternteil Core 1.15.0.1 (`ec2156cc`, mit Cores ganzem Verlauf, 2 342 Commits), zweiter Boulder 0.53
  (`aecd3350`). Der Baum ist Cores Baum mit Boulders Rahmen (`.github`, `CLAUDE.md`, `README.md`,
  `CONTRIBUTING.md`, `SECURITY.md`, `BOULDER_VERSION`, `ci/`, `LICENSE-Inter-OFL`); nicht übernommen:
  Cores `.github`, `LICENSE-COMMERCIAL`, Gravels `DESIGN_NOTES.md` und `KNOWN_ISSUES.md`.
  Abweichung von 2.1: der Merge steht am Anfang statt am Schluss, weil der Zweig ohne Force-Push
  weiterwachsen muss. `main` bleibt bis AP8 auf 0.53.
- `CLAUDE.md` ist unverändert übernommen und beschreibt noch Gravel; berichtigen kann es nur der
  Nutzer.
- **Bau:** `applicationId ch.dysseus.boulder`; versionName aus `BOULDER_VERSION`, versionCode = Zahl der
  Commits (jetzt über 2 550, 0.53 hatte 210); nur arm64-v8a; App-Name „Boulder“. Signatur wie 0.53:
  `LOCAL_RELEASE_BUILD=true` signiert mit dem festen Schlüssel aus `ci/debug.keystore`.
- **Google still (F25 A):** `androidApp/google-services.json` ist eine Attrappe für
  `ch.dysseus.boulder`; im Manifest `firebase_crashlytics_collection_enabled=false` und
  `firebase_messaging_auto_init_enabled=false`; Anmeldung (Google, Apple, GitHub) aus wie in 0.53;
  Mixpanel und Memfault ohne Token aus (Core prüft auf null).
- **CI:** JDK 17 (Core pinnt die Toolchain), Tests `testDebugUnitTest testAndroidHostTest jvmTest`.
- **Exporte:** Cores Release exportiert `androidx.compose.ui.tooling.PreviewActivity` ohne Schutz
  (jede App kann darüber Composables der App starten). Wie in 0.53 fällt ui-tooling aus dem
  Release, und `pruefe<Variante>Exporte` bricht den Release-Bau ab, wenn etwas exportiert ist,
  das nicht auf der Liste steht. Auf der Liste stehen auch Cores Ring-Teile (Teilen an „Index“,
  Boot-Empfänger) – ob sie bleiben, entscheidet AP7.
- **APK:** 36 MB; darin liegen auch iOS-Bibliotheken von Cactus (`ios/lib/…`, rund 4,5 MB gepackt),
  die Android nie lädt: Ausschluss in AP7.
- **Geprüft:** `assembleRelease` mit `LOCAL_RELEASE_BUILD` (signiert mit dem CI-Schlüssel,
  SHA-256 `212e0ca4…`; `ch.dysseus.boulder`; nur arm64; Manifest-Schalter aus; 25 erlaubte
  Exporte, keine PreviewActivity) und `testDebugUnitTest testAndroidHostTest jvmTest`: 1 817 Tests
  in Cores Modulen, keine Fehler, 9 übersprungen. Commits `3256eabfc` (Merge), `878046fbb` (Bau),
  `74b35dc2e` (`.gitignore`), `879e1fee1` (CI), gepusht auf `claude/exciting-cerf-m8apu2`.
- **CI von 0.53:** Der Lauf auf `main` (`aecd3350`) scheiterte nach grünen Tests und grünem Bau beim
  Hochladen des APK („Artifact storage quota has been hit“); darum fehlt das Release `v0.53.0`.
  Ab jetzt bleiben Artefakte nur einen Tag; die alten belegen das Kontingent, bis sie ablaufen oder
  gelöscht werden (Nutzer).
- **Offen aus AP0:** Die Schalter „Send app crashes“ und „Send watch analytics“ stehen ab Werk auf
  „an“, sammeln aber nichts (Manifest, kein Token): ausblenden in AP5/AP7. Cactus-Telemetrie
  (`cactusSetTelemetryEnvironment`) und die übrigen Netzziele: AP8.

### AP1 (14.10.2026, `9f68d7f49`)
- `libpebble3.db` zählt weiter mit Versatz: Fassung **1049** (Core 49). Cores vier Schritte 45 → 49
  laufen als **eine** AutoMigration 1045 → 1049 (Spec `AutoMigration48` löscht `LockerAppPermission`,
  F27). Einzelne Schritte bräuchten Schemas 1046–1048, die nie gebaut wurden; künftige einzelne
  Core-Schritte werden wieder je eine AutoMigration (`UPSTREAM_VERSATZ + N`).
- Boulders alte Zählung vor 0.50 (45 → 50) trug dieselben Nummern wie Cores 46–49. Im Schema-Ordner
  bleiben Cores Dateien; Boulders alte Schritte stehen als SQL in `database/boulder/Fassungen.kt`,
  ihre Schemas als Testdaten. Dort liegen auch `UPSTREAM_VERSATZ`, `DATENBANK_VERSION` und der
  `DowngradeSchutz` (statt in Cores `Database.kt`).
- Room läuft auch für die JVM (`kspJvm`, wie in 0.53), sonst fehlt den Datenbanktests `Database_Impl`.
- Tests: Migration von 0.53 (1045) mit Daten in jeder Tabelle, alte Kette 45–50, Downgrade beider
  Datenbanken, Schema-Wächter gegen Cores 49 (und 1045 gegen 45), Backup-Regeln.

### AP2 (14.10.2026, `591ffb62e`)
- Drei-Wege-Merge Gravel 0.3.3 → Boulder 0.53 auf Core 1.15 (Gravel hatte diese Dateien nicht
  geändert). Konflikte nur in `Datalogging.kt`, `DataLoggingService.kt`, `HealthDataParser.kt`,
  `LibPebble.kt`, `HealthDataEntity.kt`.
- Datalogging: Cores Weitergabe an PebbleKit-2-Begleiter bleibt; „erst sichern, dann ACK“ gilt für
  alle Pakete; bei vollem Puffer der Begleiter kein ACK statt Verwerfen. Health-Tags wie bei Core nur
  für die System-UUID, dazu Boulders Tags (88/89, 0x4001/0x4002, Quarantäne für 0x4000–0x40FF).
- Parser aus 0.53, dazu Cores Regel „Bit 0 der Sitzungsfassung gelöscht = nicht lesbar“ (steht so in
  `activity_private.h` der Firmware). Sitzungsdistanz heisst `distanceM` (Spalte `distanceCm`).
- „Populate fake health data“ ist weg (löschte alle echten Daten; `DebugDatenTest`).
- Cores `HealthDataParserTest` und `PlatformHealthSyncTest` nur an `ParsedHealthData`/`distanceM`
  angepasst. Alle Testaufgaben grün: 1 961 Tests.

### AP3 (14.10.2026, `07b567e7c`)
- **Quelle:** Core-Uhren fragen `GithubReleases` (Releases von `boulder-firmware`) statt Cores
  `EngDashOta` (dash.repebble.com, ohne Signatur); ältere Pebbles bleiben bei Cohorts. Prüfabstand 5 h
  wie in 0.53.
- **Geprüft installiert:** aus Gravel 0.3.2 mit Boulders Änderungen `VerifiedFirmwareInstaller`
  (Download mit Grössendeckel, sha256, Ed25519 über Tag, Dateiname, Grösse und sha256, PBZ-Manifest),
  `FirmwareArtifactExpectations`, `FirmwareReleaseSelection`, `ForkFirmwareInstallUi`; dazu Boulders
  `FirmwareSignatur`, `MindestApp`, `Ed25519`.
- **Alle vier Wege** gehen über den Installer: `WatchesScreen`, `WatchOnboardingScreen` (dort
  automatisch), `FirmwareUpdateUiTracker` („Update Now“), `DebugFirmwareSideload` („Update FW“).
  `FirmwareWegeTest` schlägt an, wenn Produktivcode wieder Cores `updateFirmware` ruft.
- Gravels Kanal-Schalter entfällt (die Auswahl fragt fest „Soaked“). Gravels Erwartungen aus Cohorts
  kommen nicht mit: für alte Pebbles verweigert der Installer, statt ungeprüft zu installieren (F26).
- GitHub nur mit System-CAs (`network_security_config.xml`). Alle Testaufgaben grün: 2 068 Tests.

### AP4 (14.10.2026, `bf97db520`)
- **Begleiter:** `BuiltInCompanions` aus 0.53 in libpebble3; `LibPebble.create` nimmt sie (Vorgabe
  `NoBuiltInCompanions`). Ein eingebauter Begleiter startet zuerst, auch für Systemapps ohne PBW
  (Kieselsport); neben PebbleKit JS ersetzt er den Weg zu Android-Apps, PebbleKit JS bestätigt dann
  nicht selbst. Die Begleiter selbst (Kieselsport, Drinktervall, SupCycle, Kieselstrasse) und ihr
  Einhängen in `watchModule` kommen mit AP5.
- **Fehler in Cores Weg, wie in 0.53 behoben:** Eingang je Begleiter 256 statt 64, darüber geht die
  älteste mit Log (Core verwarf die neuesten still); ein unlesbares Tupel beendete den Empfang aller
  AppMessages der Verbindung (jetzt nur diese Nachricht, mit NACK); `replyNACK` schickte ein ACK;
  eine Einstellungsseite durfte `javascript:` oder `file:` sein (Cores `mayOpen` prüft nur http/https
  gegen die Netzregeln); ein DELETE an die Timeline endete mit 500.
- **Systemapps:** Kieselsport statt Workout, dazu Kieselstrasse; TimeStyle Bedrock entfällt (F26). Ein
  bestehender Locker verliert Systemapps ohne Gegenstück in der Firmware (Workout, TimeStyle aus 0.53),
  eine Store-Kopie wird zur Systemapp, eine neue steht hinter ihrem Vorgänger.
- **PebbleKit 2:** Provider `${applicationId}.pebblekit` – Clients suchen `<Paket>.pebblekit`
  (`PebbleKitProviderContract`), Cores feste Kennung passte nur zu Cores Paket.
- **Nicht übernommen:** Gravels Rechte-Teile (F27; Core räumt die Rechte entfernter Apps selbst auf),
  Boulders Locker-Oberfläche für TimeStyle. Cores eigener Bereich „Active“ ersetzt Boulders Karussell-Fix.
- **Für später gefunden:** `boulderMessstufe` (V2 liest den Schlüssel, Einstellung „Precision“) und der
  Wetter-Wettlauf (zwei schnelle Updates, das ältere gewann) → AP5; `kotlin-reflect` aus dem Release
  samt `DevConnectionOhneReflectTest` → AP7.
- Alle Testaufgaben grün: 2 105 Tests; im Release-APK heisst der Provider `ch.dysseus.boulder.pebblekit`.

### AP5 mit AP6 (14.10.2026, `ffe0382b9`)
- **Zusammengelegt:** Die Kiesel-Teile rufen Rechtefrage und Export von `PlatformHealthSync` auf;
  getrennt hätte AP5 nicht gebaut. Darum ein Commit für beide.
- **Kiesel-Teile** (86 Dateien in `pebble/.../kiesel`, dazu Oberflächen, Ressourcen,
  `boulderSettingsItems`, Modul `:osmand-api` unverändert): aus 0.53 übernommen und ohne eine
  Anpassung gegen Core 1.15 gebaut. Begleiter (`BoulderBegleiter`) über `watchModule` an
  `LibPebble.create`.
- **Cores Health-Oberfläche entfernt** (`HealthScreen`, `HealthCharts`, `HealthStatsDialog`,
  `HealthViewModel`, `HealthUtilsTest`), wie 0.53 Gravels Fassung entfernt hatte; der Reiter Health
  zeigt zuerst die Kiesel-Ansicht. „Battery“ (Einstellungen, Uhrenmenü) führt auf Boulders
  Akkuseite; Cores Seite lädt vom Server und braucht eine Anmeldung.
- **Einstellungen:** SpO2 (Messung, Messrate, während Aktivitäten) und die Sektionen Sport,
  Navigation, Boulder dazu; „Open Google Fit“, „Show Health Tab“, „View debug stats“ und Gravels
  Kanal-Schalter weg. „Precision“ (`boulderMessstufe`) wieder in den Uhreinstellungen (V2 liest
  den Schlüssel, Werte ausserhalb 1–5 gelten wie in der Firmware als 3).
- **Health Connect:** Nächte mit Phasen, SpO2, Tagesblöcke, alle Rechte in einem Dialog, kein
  doppelter Sync, Rechtefrage nur bei Neuem – wie in 0.53 (Drei-Wege-Merge sauber).
- **Nachgeholt aus AP3:** Die Uhrenseite zeigt wieder „Firmware braucht neuere Boulder-Fassung“.
- **Wetter:** zwei schnelle Updates der Reihe nach und als Transaktion (Core: das ältere gewann).
- **Farbschema:** Boulder-Grün statt Cores Orange, vorgezogen aus AP7, weil die Kiesel-Farben
  darauf bauen (`FarbenTest`); Kontraste wie in 0.53. App-Symbol folgt in AP7.
- **Tests:** 77 Kiesel-Testdateien (Robolectric), Health-Hilfen, Sport-Unterseite, Sync ohne Nächte,
  Messstufe, Wetterreihenfolge, Kontraste. `RechteTest` liest nur die Manifeste, die Core hat.
  Alle Testaufgaben grün: 2 779 Tests; der Release-Bau mit Exportwächter auch.
- **Für AP7 gefunden:** Die Schalter „Send app crashes“/„Send watch analytics“/„Send app
  analytics“ stehen auf „an“; der erste ruft `setCrashlyticsCollectionEnabled` und schaltet
  Crashlytics nach aus/ein zur Laufzeit ein (gegen F25). Core installiert Sprachpakete (`.pbl`)
  ohne Rückfrage (Apps prüft Core im Locker, Firmware fragt nach). Effizienzteile aus 0.53
  (Arbeit nur mit Uhr, Log ab Info, Coil-Cache), Asset-Ausschluss und Cactus-`ios/lib` hängen
  teils am Entscheid zu Cores Ring/Index-Teilen.
