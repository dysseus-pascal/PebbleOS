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

## 5. Offene Fragen

- **F24 Basis:** Core 1.15.0.1 (= `master` vom 08.10.), danach eingefroren bis zum Gerätetest der App.
  Empfehlung: ja.
- **F25 Google:** Core bringt Firebase (Auth, Firestore, Crashlytics), Push (FCM), Google-Login,
  Play-Update und Mixpanel mit. A: so lassen, aber stillgelegt (Attrappe für `google-services.json`,
  keine Tokens, Sammeln aus); B: Gravels Ent-Googlung nachbauen (rund 1 600 Zeilen plus Löschungen,
  bei jedem Core-Merge zu pflegen).
- **F26 Firmware:** nur noch V2 (b27 und neuer)? Dann entfällt TimeStyle Bedrock samt Vorschau.
- **F27 PKJS-Rechte:** Cores Modell (ab Werk Netz und Standort für jedes PKJS, bis Core seine Rechte
  einschaltet) oder Gravels Sperre nachbauen.
- **F28 Spracherkennung:** Cactus (proprietär, 68 MB, nicht frei lizenziert) behalten, weglassen oder
  whisper.cpp aus Gravel (rund 12 000 Zeilen). Ohne Cactus diktiert Core über die Spracherkennung des
  Telefons (Modus `PlatformOnly`, Einstellung „System Speech Model“).
