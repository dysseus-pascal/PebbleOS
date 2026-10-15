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
- **F29** Ring/Index wie bei Core; **F30** Fassung 0.54.0; **F31** Gerätetest per APK, `main` danach.
- **Vorgabe:** Nichts über Boulders App oder Firmware an Core Devices; alles, was auf Bug-Reports oder
  Absturzprotokolle hinweist, ist ausgeblendet.

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

### AP7 (14.10.2026, `1654dcc86`, `a462a6c4e`, `3a9ef8118`)
- **Diagnose-Schalter weg:** „Send app crashes“, „Send watch analytics“, „Send app analytics“. Sie
  standen auf „an“, obwohl nichts gesammelt wird; „Send app crashes“ rief
  `setCrashlyticsCollectionEnabled(it)` und schaltete Crashlytics nach aus/ein zur Laufzeit wieder ein.
- **Sideload:** Kopie im Hintergrund statt auf dem Hauptfaden, höchstens 32 MB, Fehler gefangen (Core
  kopierte ungebremst und warf bei einem abbrechenden Strom). Sprachpakete (`.pbl`) warten auf die
  Rückfrage wie Firmware; Apps (`.pbw`) gehen wie bei Core in die Prüfung im Locker.
  `SideloadRueckfrageTest` angepasst.
- **Effizienz wie in 0.53:** Wetter und 6-h-Teil nur mit Uhr, Nachholen beim Verbinden; Log im
  Release ab Info; Coil 15 % und leer im Hintergrund; Logdatei ohne `stat()` je Zeile.
- **APK:** Cactus' `ios/lib` (16 MB ungepackt) ausgeschlossen: 36,9 → 31,7 MB.
- **Symbol:** Felsbrocken als Vektor mit Monochrom, Benachrichtigungssymbol, Logo im Onboarding.
- **Wächter** für `bestaetigt = pkjsBestaetigt(daneben)` jetzt in libpebble3 (Core hat keine Hosttests
  in `:androidApp`); er bemerkt vier eingebaute Fehler.
- **Cactus-Telemetrie aus:** Die vorgebaute Engine (`libcactus_engine.so`) schickt mit eingebautem
  Schlüssel Ereignisse an einen Supabase-Server von Cactus (`/rest/v1/logs`, `/devices`, `/projects`);
  Core startet sie bei jedem App-Start (`initCactus`). Im Maschinencode von `telemetry::init` und
  `cactus_init` liest sie `CACTUS_NO_CLOUD_TELE` (jeder Wert ausser leer und „0“ schaltet ab);
  `MainApplication` setzt ihn als Erstes. Am Binärprogramm geprüft, nicht im Betrieb gemessen.
- **Entfallen durch F29:** Asset-Ausschluss (Ring-Bilder) und `kotlin-reflect` (hängt bei Core am
  MCP-SDK des Index-Teils).

### AP8 (14.10.2026, Fassung 0.54.0, `59f939c90`, `319af1afe`, `da8a42613`)
- **Fassung 0.54.0** (F30) mit `.github/release/0.54.0.md`. Release-Build vom Stand `da8a42613`:
  versionCode 2567, `ch.dysseus.boulder`, signiert mit dem CI-Schlüssel (SHA-256 `212e0ca4…`, wie
  0.53), 31,7 MB; zum Seitenladen übergeben (F31), ersetzt die Builds 2565 und 2566. `main` bleibt
  auf 0.53.
- **Nichts an Core Devices** (Vorgabe, `319af1afe`, `da8a42613`): ausgeblendet sind „Get Help“ (New
  Bug Report, View My Bug Reports, „Getting Started & Troubleshooting“: Cores Hilfeseite führt zum
  Bug-Report-Formular), „Bug Report“ im Screenshot-Dialog der Uhr, „Create a Core Dump“, im
  Index-Teil „File Bug Report“, „Diagnostics“ und die Bug-Report-Benachrichtigungen, dazu die Deep
  Links auf Bug-Reports; `BugReportButton` zeigt nichts. Cores Bug-Report-Seiten bleiben im Code,
  ohne Einstieg. `KeinBugReportWegTest` meldet jeden Einstieg, den ein Core-Merge zurückbringt, mit
  Datei und Zeile (am wieder eingesetzten Diagnose-Eintrag belegt). Die Antwort-Benachrichtigung
  „Pebble Support message“ käme nur per Push, den Boulder nicht anmeldet. „Export logs“ aus 0.53
  (Gravels lokale Ausfuhr) gibt es nicht mehr; Cores Schalter für ausführlichere Logs bleiben.
- **Prüfung:** alle Testaufgaben grün, 2 798 Tests (11 übersprungen, wie bei Core), dazu der
  Exportwächter im Release-Bau.
- **Testwettlauf** (`b5e806882`, 15.10.): Der CI-Lauf von 0.53 scheiterte zweimal in
  `BegleiterAblaufTest` (`ein_glas_dessen_erster_versuch_...`) nach 10 s. Der Arbeiter versucht das
  Eintragen, schreibt dann ins Protokoll und benennt die Datei erst danach als gemeldet um; ein
  zweiter Start in dieser Lücke fasste den alten Namen, sein Auftrag lief ins Leere. In der App
  harmlos (die Datei wartet bis zum nächsten Start). Die Tests für Glas und Training warten jetzt auf
  den Vermerk im Namen, wie es der Nachbartest schon tat; `warte()` nennt bei einer
  Zeitüberschreitung die Bedingung. Belegt mit 300 ms Verzögerung im Arbeiter (nur für die Probe):
  vorher genau diese zwei Fehler wie in der CI, danach 17 von 17 grün.
- **Release-Notiz** (`77975acc7`): nennt den Gerätetest statt „nicht auf deinem Telefon geprüft“.

#### Netzwerkziele (aus dem Code, nicht im Betrieb gemessen)
| Ziel | Wann | Bemerkung |
|---|---|---|
| `api.github.com`, `github.com`/`objects.githubusercontent.com` | Firmware-Prüfung alle 5 h, Download beim Installieren | nur `dysseus-pascal/boulder-firmware` |
| `weather-api.repebble.com` | Wetter: im Hintergrund nur mit verbundener Uhr, beim Öffnen der App, falls noch nicht geholt | Cores Wetterdienst, mit Ort |
| `appstore-api.repebble.com`, `appstore-api.rebble.io`, `binaries.rebble.io`, `assets2.rebble.io`, `apps.repebble.com` | Store und Locker beim Öffnen, Installieren | Core- und Rebble-Store |
| `notif-app-icons.repebble.com` | Liste der Benachrichtigungs-Apps | Symbole |
| `translate.repebble.com` | Sprachpaket laden | auf Wunsch |
| `huggingface.co` | Cactus-Modell laden | auf Wunsch (Offline-Diktat) |
| `cohorts.rebble.io` | Firmware-Prüfung alter Pebbles | nicht Boulders Uhr |
| `nominatim.openstreetmap.org`, OSM-Kacheln (osmdroid) | Ortssuche von Kieselstrasse, Trainingskarte | auf Wunsch |
| Uhr-Apps (PKJS) | je App | nach Cores Rechtemodell (F27) |
| Wispr (`wispr.ai`, `platform-api.wisprflow.ai`) | nur wenn eingerichtet | Diktat-Dienst |
| Index/Ring: `nenya.repebble.com`, `index-oauth.repebble.com`, Supabase (Core), Notion, Google Tasks | nur mit gekoppeltem Ring | F29 |
| Google (Firebase), Mixpanel, Memfault, Cores Analysen (`bugUrl`) | nicht | Attrappe, Sammeln aus im Manifest, keine Tokens, `bugUrl` leer (F25) |
| Cores Bug-Reports, Core Dumps, Support | nicht | kein Einstieg (Vorgabe), `bugUrl` leer, kein Push-Token |
| Cactus-Telemetrie (Supabase) | nicht | `CACTUS_NO_CLOUD_TELE` |

#### Stand der Abnahmekriterien (Abschnitt 4)
1. Basis: der Zweig enthält Core 1.15.0.1 mit Verlauf; Exportwächter grün. `main` folgt nach dem
   Gerätetest.
2. Update ohne Verlust: gleiche App-ID und Signatur, versionCode 2567 > 0.53; Migration 1045 → 1049
   mit einer Zeile in jeder Tabelle (`DatenbankFassungTest.migration1045`). Ausnahme: Gravels
   PKJS-Freigaben (F27).
3. Daten der Uhr: Parser und Wege wie in 0.53 getestet (AP2); auf dem Telefon kam die Nacht
   vom 14. auf den 15.10. mit Phasen an (Nutzer).
4. Boulders Teile: alle übernommenen Tests grün; auf dem Telefon offen.
5. Firmware: AP3, `FirmwareWegeTest`.
6. Core bleibt Core: Cores Tests grün; Benachrichtigungen, PKJS, Locker, Wetter und Kalender auf dem
   Telefon offen.
7. Netz: Liste oben aus dem Code; im Betrieb nicht gemessen.
8. Release und Gerät: Release-Build signiert mit dem CI-Schlüssel. Installation über 0.53: bestanden
   (Nutzer, 14.10.: 2567 „installiert und läuft“). Eine Nacht der Uhr: bestanden (15.10., „nacht
   ist da, mit phasen“). Release `v0.54.0` am 15.10. um 20:40 UTC über die CI (Lauf 80, `02f06f2c5`,
   versionCode 2572, APK 31,6 MB): erfüllt. Offen bleiben die Netzliste im Betrieb (7) und, auf dem
   Telefon nicht einzeln bestätigt, Benachrichtigungen, Locker, Firmware-Prüfung, Widget und Health
   Connect (4, 6).

#### Für den Gerätetest
- Vorher in Boulder 0.53 eine Sicherung machen. Zurück auf 0.53 (`adb install -d`) legt 0.53 die
  neuere Datenbank nach `databases/beiseite` und startet leer.
- Uhr auf V2 (b27 oder neuer; F26).
- Prüfen: Start ohne Absturz, Health mit den alten Daten, eine Nacht kommt an, Benachrichtigungen,
  Locker (Kieselsport, Kieselstrasse), Firmware-Prüfung, Widget, Health Connect.
- **Stand 14.10. abends:** 2567 über 0.53 installiert, startet und läuft (Nutzer). Die erste Nacht
  steht aus; danach `main` (Fast-Forward von `aecd3350`) und Release über die CI (F31).
- **Stand 15.10.:** Die Nacht kam mit Phasen an, der Gerätetest ist bestanden. `main` steht seit
  18:07 UTC per Fast-Forward auf `77975acc7` (0.54.0, versionCode 2569); CI-Lauf 77.

#### Sperre in der CI (14.10.)
- Der Lauf von 0.53 auf `main` scheiterte beim Hochladen des APK: „Failed to CreateArtifact:
  Artifact storage quota has been hit … Usage is recalculated every 6-12 hours.“ Darum fehlt
  `v0.53.0`. Die APKs der früheren Läufe liegen mit 90 Tagen Aufbewahrung (je etwa 6,8 MB, etwa
  0.52 bis Januar 2027). `retention-days: 1` steht erst im Zweig und wirkt nur für neue Artefakte.
- Ein Push nach `main` scheitert genauso, bis der Nutzer die alten Artefakte löscht (die Werkzeuge
  hier können keine Artefakte löschen) und GitHub neu gerechnet hat. Danach: Lauf 76 (0.53) neu
  starten für `v0.53.0`, dann `main` vorziehen für `v0.54.0`.
- **15.10.:** Der Nutzer löschte gegen 05:35 UTC die 36 grünen Läufe samt APKs (76 → 40 Läufe). Die
  übrigen Repos halten kaum Artefakte (Uhr-Apps je etwa 0,3 MB; die Firmware-Läufe im PebbleOS-Fork
  scheiterten alle ohne Artefakt, die Workflows dort sind abgeschaltet). Lauf 76 in fünf weiteren
  Versuchen: zweimal der Testwettlauf (nur im Stand von 0.53, oben), dreimal grün bis zum Hochladen,
  das noch am Kontingent scheiterte (08:09, 12:02, 13:22 UTC – knapp 8 h nach dem Löschen nicht
  verrechnet). **`v0.53.0` entfällt** (angekündigt für den Fall des Wettlaufs; 0.54 ersetzt 0.53).
- Ausweg, falls das Kontingent hängen bleibt: die Übergabe an den Release-Job über den Cache statt
  über Artefakte (der Cache zählt nicht zum Kontingent); die Rechtetrennung bleibt.

#### Die ersten CI-Läufe von 0.54 auf `main` (15.10.)
- **Lauf 77** (`77975acc7`): neun Tests rot mit `UnsupportedClassVersionError` –
  `com/viktormykhailiv/kmp/health/HealthDataType` hat Klassendatei-Fassung 65, health-kmp 1.4.0 ist
  ganz für Java 21 übersetzt. AP0 hatte die CI auf JDK 17 gestellt wie Core; Cores Tests berühren
  die Bibliothek nicht, Boulders Health-Tests schon (0.53 lief noch auf 21). Lokal mit JDK 17 genau so
  nachgestellt. **Fix `933b5aeeb`:** CI auf JDK 21; Cores `jvmToolchain(17)` nimmt das JDK 17 des
  Runners.
- **Lauf 78:** Tests grün, dann „The runner has received a shutdown signal“ drei Minuten in R8. Der
  Runner hat 7 GB; der Kotlin-Daemon der Tests (alle Module, bis 6 GB Heap) lief neben Gradle (4 GB).
  Lokal gemessen (Summe der Java-Prozesse): 6,4 GB wie bisher, 4,6 GB mit frischem Gradle, Kotlin im
  Prozess und `-Xmx3g`, gleiche Dauer. **Fix `dda06b7e5`:** so im Bauschritt.
- **Lauf 79:** Tests und Bau grün, Hochladen wieder am Kontingent (20:17 UTC, knapp 15 h nach dem
  Löschen). **Fix `02f06f2c5`:** APK über `actions/cache` (Schlüssel `boulder-apk-<sha>`, nur bei
  neuer Fassung) an den Release-Job, `fail-on-cache-miss`.
- **Lauf 80:** grün, Release `v0.54.0` (Titel „0.54.0 - Neu gebaut auf der App von Core Devices“,
  `boulder.apk` 31 638 612 Bytes).
- Das Artefakt-Kontingent des Kontos bleibt voll, bis GitHub neu rechnet; bis dahin scheitert auch
  das Hochladen von Artefakten in den Repos der Uhr-Apps (Drinktervall, SupCycle).
