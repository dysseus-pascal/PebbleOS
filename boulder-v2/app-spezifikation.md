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
