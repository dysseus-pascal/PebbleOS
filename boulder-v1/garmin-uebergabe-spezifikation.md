# Garmin-Übergabe – kleine Android-App (Spezifikation, 07.10.2026)

**Zweck:** Der Garmin-Datenexport des Nutzers enthält ältere Körper- und Blutdruckwerte. Die App schreibt sie einmalig rückwirkend nach Health Connect, denn Garmin trägt dort keine alten Werte nach. Boulder liest sie danach von selbst; Boulder liest Health Connect aus allen Quellen.

## Eingang
Eingang ist das ZIP `DI-Connect-Wellness` aus dem Garmin-Export (Ordner `DI_CONNECT/DI-Connect-Wellness`). Echte Kopie zum Prüfen: `/tmp/claude-1000/-home-dysseus-Documents-Claude/a0d87cde-04f6-43b4-bc17-a2150aed9501/scratchpad/garmin/`, flüchtig. Das sind persönliche Gesundheitsdaten: nie ins Repository, nie ins Netz. Testdaten sind nachgebaut.

1. **`<id>_userBioMetrics.json`:** eine Liste. Einträge mit `weight` haben diese Felder:
   - `weight.weight` in Gramm, `weight.timestampGMT` (UTC, Form `2024-05-01T06:12:00.0`) und `weight.sourceType`;
   - bei Waagen-Einträgen zusätzlich `bodyFat` (%), `bodyWater` (%), `boneMass` (g), `muscleMass` (g) und `bmi`.
   - Echte Zahlen: 82 Gewichte von 2022-03 bis 2026-10; 69 davon von `INDEX_SCALE` mit Zusammensetzung, 9 von `MFP`, 2 `MANUAL`, 1 `CHANGE_LOG`, 1 `USER_SETTING`.
   - Werte 0 bedeuten, dass die Waage nichts gemessen hat.
2. **`BloodPressureFile_<von>_<bis>.json`, mehrere Dateien:** eine Liste mit `bloodPressure.systolic`, `.diastolic` (mmHg), `.pulse` und `metaData.calendarDate` als `[J, M, T, h, m, s]`.
   - Die Zeit ist sehr wahrscheinlich LOKAL: Die Messung vom 02.10.2026 07:17 steht in Boulder als 07:11 aus einer anderen Quelle.
   - Echte Zahlen: 11 Messungen (2024 und 2026).

## Was geschrieben wird
| Garmin | Health Connect | Regel |
|---|---|---|
| Gewicht (g) | `WeightRecord` (kg) | nur Quellen `INDEX_SCALE`, `MFP`, `MANUAL`. `CHANGE_LOG` und `USER_SETTING` sind Profilangaben, keine Messungen. |
| bodyFat % | `BodyFatRecord` | nur Werte > 0 |
| boneMass g | `BoneMassRecord` (kg) | nur Werte > 0 |
| bodyWater % | `BodyWaterMassRecord` (kg) | Gewicht × % / 100, nur > 0 |
| Blutdruck | `BloodPressureRecord` | Zeit lokal (Systemzone); Körperlage und Messort unbekannt |
| muscleMass, bmi, pulse | – | Health Connect hat keinen passenden Typ beziehungsweise braucht ihn nicht. Bleibt weg. |

## Regeln
- **Kein Doppel aus erneutem Lauf:** Jeder Satz hat eine feste `clientRecordId`, zum Beispiel `garmin-gewicht-<epochSekunden>`, und eine feste `clientRecordVersion`. Ein zweiter Lauf schreibt so nichts doppelt; Health Connect behandelt die Id als Upsert.
- **Kein Doppel zu Vorhandenem:** Vor dem Schreiben liest die App, was im Zeitraum schon steht, egal aus welcher Quelle. Liegt ein Satz derselben Art höchstens ±10 min neben einem Garmin-Wert, wird dieser übersprungen und als „already there“ gezählt. Das betrifft Werte, die Garmin seit dem Einschalten der Verbindung selbst geschrieben hat.
- **Nie löschen, nur einfügen.** Die App fragt nur Lese- und Schreibrechte für genau diese 5 Arten an. Ohne Recht für eine Art wird diese übersprungen, die anderen laufen weiter.
- **Ablauf:**
  1. ZIP über den Dateiauswahl-Dialog wählen (SAF).
  2. Vorschau: je Art die Anzahl gefunden, schon vorhanden, neu, dazu der Zeitraum.
  3. Knopf „Write N entries“.
  4. Ergebnis je Art.
- **Schreiben:** in Blöcken. Jeder Fehler wird geloggt und im Ergebnis gezeigt, nichts wird still verschluckt.
- **UI:** Englisch, schlicht, ein Bildschirm.
- **Kein Netz:** keine Internet-Berechtigung.

## Technik
- **Projekt:** neues Gradle-Projekt `~/Projects/Garmin-Uebergabe`, git lokal, noch kein GitHub; das entscheidet der Nutzer.
- **Bauweise:** Kotlin, Compose, minSdk 28 (Health Connect), `androidx.health.connect:connect-client:1.1.0` wie Boulder. Paket z. B. `io.github.dysseuspascal.garminimport`.
- **Manifest:** Health-Connect-Rechte und die Rationale-Activity für Health Connect.
- **Release-APK:** signiert mit dem Debug-Schlüssel aus `~/Projects/Boulder/ci/debug.keystore`, damit sie installierbar ist. Es ist eine eigene App, kein Update von Boulder.
- **Tests:**
  - Parser mit nachgebauten JSON-Dateien in derselben Form: Gramm → kg, % → Wassermasse, 0 → weg, Quellenfilter, lokale Zeit beim Blutdruck, Zuordnung der Zusammensetzung zum Gewicht.
  - Abgleich ±10 min.
  - Stabile Ids.
  - Kernaussagen mit Mutation belegen.
- **Selbsttest am echten Export, nur lokal:** Zählen, was die App aus dem echten Export lesen würde. Erwartet sind 80 Gewichte, 11 Blutdruckmessungen, bis zu 69 Fett/Knochen/Wasser abzüglich der Nullen.
- **Zeit- und Zonenangaben:** Gewicht immer UTC, Blutdruck in der Systemzone.
- **Belege:** Robolectric-PNG des Bildschirms mit nachgebauter Vorschau.
