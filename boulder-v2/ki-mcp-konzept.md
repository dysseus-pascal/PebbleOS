# KI-Einbindung über MCP – Konzept (16.10.2026)

Auftrag (Nutzer, 16.10.): „sieh dir den upstream an und überlege dir die ki einbindung über einen
mcp“. Vorläufer: „Coach“ in `ideen.md` (05.10., vorerst nicht).

## 1. Was Upstream mitbringt (Core 1.15.0.1)

`core/master` steht seit dem 08.10. auf `ec2156cc3` (1.15.0.1), unserer Grundlage. Danach kam nur
ein README-Entwurf zur Architektur (Zweig `readme-architecture`); der letzte Commit auf `master`
ändert ein Etikett im Locker. Befund aus dem Code (Belege mit Datei und Zeile im Bericht vom 16.10.):

- **`mcp/` ist nur ein MCP-Client** (kotlin-sdk 0.15.0, Streamable HTTP und SSE, ein fester
  `Authorization`-Header, kein OAuth). Daneben „eingebaute“ Werkzeuggruppen im Prozess
  (`BuiltInMcpIntegration`, `McpTool`), die die Typen des SDK nutzen, aber kein Protokoll sprechen.
- **Kein MCP-Server** im Produktcode; nur ein Testserver unter `jvmTest`. Die SDK-Fassung bringt die
  Server-Transporte aber mit, und ktor-server (CIO) ist schon eine Abhängigkeit der App
  (DevConnection).
- **Der KI-Assistent ist der des Index-Rings** (`index-ai`, `experimental`). Zwei Wege:
  - Cloud „Nenya“ (`nenya.repebble.com`, Firebase-Token, Modell serverseitig gewählt): ohne
    Core-Anmeldung tot, und er schickte die Daten an Core. Für Boulder ausgeschlossen (Vorgabe vom
    14.10.).
  - Auf dem Gerät Cactus `needle-pebble-ft`: ein Durchgang, 256 Tokens, ohne Kontext, nur Notizen,
    Erinnerungen, Wecker. Für Auswertungen von Gesundheitsdaten zu schwach.
- **Plugins** sind Pebble-Plugins (JS in `.pbw` oder Kotlin) mit Quellen und Aktionen. Seit
  `bd238f090` erscheinen sie dem Agenten als Werkzeuge im Prozess; standardmässig aus.
- **Die Uhr spricht mit keiner KI.** Diktat ist nur Sprache-zu-Text. In Boulder ist es heute ganz
  aus: Der Standard „RemoteOnly“ hat ohne Core- oder Rebble-Anmeldung kein Backend.
- **Core hat keine Gesundheitswerkzeuge** für die KI.

Folgerung: Von Upstream taugen die MCP-Bibliothek (Client und Server) und die Werkzeugtypen. Den
Assistenten selbst übernehmen wir nicht.

## 2. Was Claude heute kann (Stand 16.10.2026)

- **Connectors in den Claude-Apps** (Web, Desktop, Android, iOS): Sie verbinden sich „from
  Anthropic's cloud, not from your local device“, der Server muss „reachable over the public
  internet“ sein. Anmeldung per OAuth, ohne Anmeldung oder mit einem festen Header. Auf dem Handy
  nutzbar; Einrichten dort noch Beta, sonst über Web oder Desktop.
- **Lokale MCP-Server** laufen in Claude Desktop (stdio, Desktop-Erweiterungen) und in Claude Code
  (auch HTTP-Server im LAN mit Header). Beide laufen auf dem PC.
- **Claudes eigene Health-Connect-Anbindung** auf Android liest Schritte, Training, Puls, HRV, SpO2,
  Schlaf und mehr. Sie ist Beta, nur Pro/Max, Android 14+, nur lesen und „Currently limited to users
  in the US“ (Hilfeseite, 25.08.2026). Boulder schreibt in Health Connect; käme sie in die Schweiz,
  bräuchte es dafür nichts.
- **Claude-API mit eigenen Werkzeugen:** Die App ruft Claude auf, die Werkzeuge laufen auf dem
  Telefon. Nur die Frage und die angeforderten Ergebnisse gehen an Anthropic. Kosten je Token,
  getrennt vom Claude-Abo (je Mio. Tokens Ein-/Ausgabe: Opus 5.5 $4/$20, Sonnet 5.5 $2/$10,
  Haiku 5.5 $0.10/$0.50).
- Der MCP-Connector der API braucht ebenfalls eine öffentliche URL.
- Quellen: [Connectors](https://support.claude.com/en/articles/11176164-use-connectors-to-extend-claude-s-capabilities),
  [Claude mit Android-Apps](https://support.claude.com/en/articles/11869629-use-claude-with-android-apps).

**Kernpunkt:** Ein MCP-Server auf dem Telefon erreicht die Claude-App nur, wenn er öffentlich im
Internet steht. Es kommt also darauf an, wo die KI läuft.

## 3. Die Wege

| | Weg | KI läuft | Daten gehen | Aufwand | Haken |
|---|---|---|---|---|---|
| A | **Claude in Boulder** (API, Werkzeuge auf dem Telefon) | Anthropic | Telefon → Anthropic, nur Angefragtes | mittel | eigener API-Schlüssel, eigene Kosten, Oberfläche in Boulder statt in der Claude-App |
| B | **MCP-Server im WLAN** | Claude Desktop / Code am PC | Telefon → PC → Anthropic | mittel | nur zu Hause am PC; im LAN ohne TLS nur mit Token |
| C | **Öffentlicher Connector über ein eigenes Relay** | Claude-App überall | Telefon → Relay → Anthropic | hoch, dazu Betrieb | Gesundheitsdaten über einen öffentlichen Dienst; Relay betreiben und absichern |
| D | Claudes Health-Connect-Anbindung | Claude-App | Health Connect → Anthropic | keiner | nur USA |
| E | Datei (Minuten-, Nachtexport) | Claude-Chat | von Hand | keiner | von Hand |

Ein Tunnel direkt zum Telefon (Cloudflare, ngrok, Tailscale Funnel) statt eines Relays ist auf
Android wackelig (Akku, Hintergrund) und lässt je nach Dienst einen Dritten mitlesen.

## 4. Vorschlag

**Eine Werkzeugschicht, mehrere Türen.** Boulders Daten werden einmal als MCP-Werkzeuge beschrieben
(Typen des kotlin-sdk wie in Cores `mcp/`). Jede Tür nutzt dieselben Werkzeuge.

1. **Werkzeugschicht** (zuerst, mit Tests): nur lesen, zusammengefasst, mit Grenzen.
   - `tage(von, bis)`: je Tag Schritte, Distanz, aktive kcal, Ruhepuls, HRV, Schlaf und Phasen,
     Wasser, Koffein, Trainingsminuten. Höchstens 90 Tage.
   - `nacht(datum)`: Einschlafen, Aufwachen, Phasen, HRV-Fenster, Ruhepuls, SpO2, Akku.
   - `trainings(von, bis)`: Sportart, Dauer, Strecke, Puls (Mittel, Maximum, Zonen), kcal.
   - `puls(von, bis)`: Puls in 5-Minuten-Medianen, höchstens 24 h.
   - `minuten(von, bis)`: Rohminuten wie der Minutenexport, höchstens 6 h.
   - `ziele()` und `uhr()`: Ziele, Maximalpuls, Einheiten; Firmware, Akku, letzte Verbindung.

   Ausgaben kompakt, mit Einheiten und Ortszeit samt Versatz. Freigabe je Kategorie in den
   Einstellungen, jeder Abruf im Protokoll, ein Schalter für alles.
2. **Erste Tür: A, Claude in Boulder.** Kein Server, kein Dritter, unterwegs nutzbar. Eine Seite
   „Ask Claude“ mit Antworten aus den Werkzeugen. Später auch von der Uhr: Diktat (lokal mit Parakeet
   statt „RemoteOnly“), Antwort als Benachrichtigung.
3. **Zweite Tür, wenn gewünscht: B.** Wenig Aufwand, sobald die Werkzeuge stehen (Server-Transport des
   SDK, ktor schon da). Nur im WLAN, nur eingeschaltet, mit zufälligem Token, Abrufe im Protokoll.
4. **C nur auf ausdrücklichen Wunsch.** Das ist der einzige Weg in die Claude-App. Er verlangt ein
   eigenes Relay (Telefon baut die Verbindung auf, Claude meldet sich mit festem Header an) und dessen
   Betrieb.
5. **D beobachten:** Kommt Claudes Health-Connect-Anbindung in die Schweiz, liest sie Boulders Daten
   ohne eigenen Code.

Nicht vorgesehen: Cores Nenya (Daten an Core), Cactus als Analyst (zu schwach), Schreibwerkzeuge
(erst nach einem eigenen Entscheid).

## 5. Offene Entscheide

- **F34:** Welche Tür zuerst – A (Vorschlag), B oder C?
- **F35:** Welche Kategorien darf die KI lesen? Vorschlag: alle, ausser Rohminuten nur auf Nachfrage.
- **F36** (bei A): Modell und Kosten. Standard ist Claude Opus 5.5; günstiger sind Sonnet 5.5 und
  Haiku 5.5. Dazu ein eigener API-Schlüssel aus der Claude Console.

## 6. Nebenbefunde

- Diktat auf der Uhr geht in Boulder laut Code nicht (kein Sprache-zu-Text-Backend ohne Anmeldung;
  auf dem Gerät nicht geprüft).
  Abhilfe wäre „LocalOnly“ mit Parakeet; das lädt ein Modell von Hugging Face.
- Cores DevConnection lauscht, wenn eingeschaltet, auf `0.0.0.0:9000` ohne Anmeldung. Standardmässig
  aus; ein Grund mehr, einen eigenen Server nur mit Token und nur im WLAN zu betreiben.
- Die Gravel-Hälfte von Boulders `CLAUDE.md` beschreibt nicht mehr den Bau (Firebase-Stubs, whisper,
  `experimental` ausgesteckt). Ändern kann sie nur der Nutzer.

## 7. Entscheid und Bau: eigener Server in Podman (16.10.2026)

**F34** (Nutzer): „wir machen eine kleine server applikation. sagen wir etwas das in podman läuft“.
Dazu Spiegel auf dem Server und Zugang über einen Cloudflare Tunnel. Damit wird aus Weg C ein selbst
betriebener Server statt eines fremden Relays. Die Entscheide stehen in `entscheidungen.md`.

Gebaut für Boulder 0.55.0:

- **Module im Boulder-Repo:** `austausch/` (das Format, KMP für App und Server) und `server/`
  (Kotlin/JVM, Ktor CIO, SQLite, Server-Teil des kotlin-sdk 0.15.0).
- **`POST /sync`** mit dem Sync-Token: Boulder schickt neue oder geänderte Tage, Nächte, Trainings
  und auf Wunsch Minuten. Der Server ersetzt per Upsert (Schlüssel Datum, Beginn, Kennung, Minute);
  Minuten hält er 90 Tage.
- **`/mcp`** (Streamable HTTP, zustandslos) mit sechs nur lesenden Werkzeugen: `stand`, `tage`
  (≤ 366 Tage), `nacht`, `trainings`, `puls` (5-Minuten-Blöcke, ≤ 24 h), `minuten` (≤ 6 h). Zugang
  mit dem MCP-Token als festem Header oder per OAuth. Auf Wunsch lässt der Server an `/mcp` nur
  Anthropics Netz 160.79.104.0/21 zu.
- **OAuth 2.1** für die Claude-App, weil feste Header dort eine Beta für einige Organisationen
  sind. Der Server bietet Metadaten nach RFC 9728/8414, Registrierung nur für Claudes Rückweg oder
  Loopback, PKCE S256, Zugang 1 h, Erneuerung 90 Tage mit Rotation und eine Sperre nach 10
  Fehlversuchen. Gespeichert ist nur ein HMAC mit dem MCP-Token, ein neues Token meldet also alle
  ab. Die Anmeldeseite fragt das MCP-Token.
- **Betrieb:** `:server:distTar` baut das Bündel `boulder-server-<fassung>.tar.gz`: bin/lib,
  Containerfile (Temurin 21 JRE, Nutzer 10001), Quadlet-Pod (Server und cloudflared, kein
  veröffentlichter Port), Volume, drei Podman-Secrets als Dateien. Die Anleitung steht in
  `server/LIESMICH.md`.
- **Boulder:** unter Settings → Boulder server. Eingetragen werden Adresse (nur https) und Sync-Token.
  Dazu kommen Schalter je Art und die Knöpfe „Send now“, „Send everything again“ und „Remove“.
  - Boulder sendet eine Minute nach jedem Schub der Uhr, sonst alle 30 Minuten.
  - Gesendet wird nur Geändertes (Abdruck je Tag, Nacht und Training). Als gesendet gilt es erst
    nach der Quittung.
  - Die Token-Datei geht in kein Backup und zu keinem Geräteumzug mit.
- **Daten:** liegen auf dem eigenen Server. Cloudflare sieht den Verkehr, weil TLS dort endet.
  Claude bekommt nur, was ein Werkzeug liefert. Nichts geht an Core oder Google.

## 8. Review von 0.55.0 behoben, Fassung 0.56.0 (22.10.2026)

Die Review mit `/code-review` fand 15 Befunde und kleinere. Alle sind behoben, auf
`claude/exciting-cerf-m8apu2` in Boulder, je mit Test und einem gezielt eingebauten Fehler belegt.

- **Format (`austausch/`):** neu sind `Tag.entfernt`, `Nacht.geladen` und
  `Lieferung.trainingsEntfernt`, alle mit Vorgabe. Ein Server mit 0.55.0 liest die neue App und
  umgekehrt.
- **Server:**
  - Tage werden zusammengeführt statt ersetzt. `entfernt` löscht einzelne Spalten, ein leerer Tag
    fällt weg. `trainingsEntfernt` löscht Trainings.
  - `nacht` nimmt die längste Nacht des Aufwachtags, wie `Gesundheitsrechnung.jeTag`, und sagt,
    wenn die Uhr nachts lud.
  - Die Werkzeuge erlauben `bis` bis 366 Tage nach `von`. Eine Ortszeit in der Lücke der
    Sommerzeit zählt als der Sprung. Die Zone wird einmal je Anfrage gelesen, und die Fehlertexte
    sind englisch.
  - Anmeldung: Die Client-Kennung ist Base64(JSON) plus HMAC mit einem Schlüssel aus dem MCP-Token.
    Registrieren schreibt also nichts, und Clients aus 0.55.0 liest der Server weiter aus der
    Tabelle. Die Sperre nach 10 Fehlversuchen gilt je Absender (IPv6 je /64); gemerkt werden
    höchstens 10'000 Absender.
  - SQLite kommt aus `androidx.sqlite:sqlite-bundled` statt sqlite-jdbc. Das Bündel ist damit
    16 statt 24 MB gross.
- **App:**
  - Der Minutenstand folgt der jüngsten Minute bis jetzt (`HealthDataApi.getLetzteMinuteBis`).
  - Tage gehen auch ohne ganze Akte. `Trendstand.akteGanz` sagt je Spalte, ob sie ganz gelesen
    ist; im 180-Tage-Fenster nennt ein Tag eine ganz gelesene, aber fehlende Spalte als entfernt.
  - Trainings: Der Puls gehört zum Abdruck. Entfernt wird nur im gelesenen Fenster und im
    Hintergrund ohne Sonderrecht nur Boulders eigene ("@b" im Stand). "Alles" behält die
    gesendeten mit leerem Abdruck.
  - Ein **gründlicher Lauf** (einmal am Tag, nach "Alles", für einen neuen Server und mit
    "Send now") vergleicht Trainings über 90 Tage und alle Nächte. Ein gewöhnlicher vergleicht
    Trainings über 3 Tage und Nächte über 7 Tage und rechnet die Tage höchstens stündlich.
  - WorkManager nur über `arbeit()` (try/catch Throwable). `plane` berührt ohne Server
    WorkManager gar nicht.
  - Kam ein Schub, während ein Anstoss lief, folgt ihm ein weiterer (`nachgestossen`,
    APPEND_OR_REPLACE).
  - Den Sendestand schreibt ein Senden nur unter der Sperre `stand` und nur, wenn die Adresse noch
    gilt.
  - Die Felder der Seite hängen nicht mehr an `stand`. Ein Fehler auf dem Telefon gilt nicht als
    vorübergehend.
  - Aufgeräumt nach R01, R04, R05, R06 und R09; R02, R03 und R08 betrafen den Server.
- **LIESMICH:** sagt jetzt, was "Send everything again" zurückbringt. Der Tokenwechsel zeigt das
  neue Token. Die Sperre gilt je Anschluss, und erst kommt der Server, dann die App.
- **CI:** Das Bündel wird immer gebaut. Der Release-Job entscheidet nach den Tests; ein lokal
  angelegtes Release bekommt das Bündel, wenn sein Tag auf den gebauten Stand zeigt.
- **Geprüft:**
  - 2891 Tests, 11 davon wie bei Core übersprungen.
  - Der Server im Podman-Container (Temurin 21) mit Lieferungen, Werkzeugen, OAuth, einem Neustart
    und einem Client aus 0.55.0.
  - Der CI-Schritt gegen ein nachgestelltes gh in sechs Fällen.
