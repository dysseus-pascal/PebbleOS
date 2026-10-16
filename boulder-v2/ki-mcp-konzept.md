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
