# Boulder 0.53 (13.10.2026)

Punkt 3 aus Abschnitt 7 des Abnahmeberichts (`../../schritt3-abnahme.md`, Abschnitt 11). Boulder-Zweig
`claude/exciting-cerf-m8apu2`, fünf Commits auf 0.52.0 (`34fc13cb`):

| Commit | Inhalt |
|---|---|
| `36fad7e2` | Gravel 0.3.4 (zurückgenommen in `aecd3350`) |
| `2916e3f0` | Firmware V2 erkennen: dort ohne TimeStyle Bedrock |
| `fcd56ca8` | Goals: Schritt- und Schlafziel einstellbar, auch für die Uhr |
| `23b335a9` | Fassung 0.53.0: Release-Notiz und Versionsnummer |
| `aecd3350` | Gravel 0.3.4 wieder entfernt: Boulder baut nicht mehr auf Gravel auf (Entscheid 13.10.) |

## Gegenproben (eingebauter Fehler → Tests rot)

| Fehler | rot |
|---|---|
| Locker: Hinweis „Needs Boulder firmware“ auch auf V2 | 1 (`boulderV2OhneKachel`) |
| Bedrock: Build auch für V2 | 2 (`buildAusDerFassung`, `zurueckVonV2EinmalNeuGesetzt`) |
| Widget mit fester Schrittzahl | 1 (`ziele_aus_goals_gelten_im_satz_und_im_widget`) |
| Tagessatz mit festem Schlafziel | 1 (dieselbe) |
| Vergleich mit fester Schrittzahl | 1 (`schwellen_folgen_den_zielen`) |

## Dateien

| Datei | Inhalt |
|---|---|
| `tests.txt` | Testzahlen nach `clean`, jede Testaufgabe mit `--rerun`, und die neuen Testklassen |
| `ziele.png` | Die Seite Goals, in Robolectric gezeichnet (graues Schema, 412 dp) |

## Umgebung

Android-SDK 37, Build-Tools 37.0.0, NDK 28.2.13676358, CMake 3.22.1, Gradle 9.6.1, JDK 21. Maven
Central antwortet über den Proxy mit 429; ein lokales Init-Skript (`~/.gradle/init.d/spiegel.gradle`)
setzt Googles Spiegel davor. Robolectrics `android-all-instrumented` liegt vorab in `~/.m2`. Beides
gehört nicht ins Repo.
