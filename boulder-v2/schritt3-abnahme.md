# Boulder V2 – Schritt 3: Abnahme (AP9)

Stand 12.10.2026, Zweig `v2` bei `ecb6b739`, Notizen im Zweig `notizen`.
Grundlage: Spezifikation `schritt3-rest-spezifikation.md`, Abschnitte 5 (AP9) und 7 (Kriterien, F15).
Belege: `belege-schritt3/ap9/`.

## 0. Kurzfassung

- **Kriterien:** 1, 2, 3, 4, 5 und 7 sind erfüllt (Host, QEMU, Abbild). 6 ist teilweise erfüllt: in QEMU
  ja, auf der Uhr steht der Gerätetest aus. 8 ist teilweise erfüllt: auf der Uhr ja, in Boulder 0.53 fehlt
  die Seite „Goals“. *Nachtrag 13.10.: Boulder 0.53 hat die Seite (Abschnitt 11); veröffentlicht ist es
  noch nicht.*
- **Gegenprüfung:** Drei Prüfer meldeten 23 Befunde, davon 21 verschiedene. Alle habe ich am Code
  bestätigt und behoben, darunter eine Sicherheitslücke an der Syscall-Grenze (K1) und ein
  Use-after-free (A1). Zu jeder Behebung gibt es einen Test, der ohne sie rot ist.
- **AP9 selbst** fand zwei Dinge:
  - Der erste Start nach dem Boot schwankt in Echtzeit-QEMU zwischen 30 und 54 ms. Das ist QEMUs
    Übersetzen beim ersten Lauf, nicht die App: Mit `-icount` sind es 17,6 ms, mit dem Code von AP8
    16,6–17,6 ms.
  - Die QEMU-Bilder zeigten auf Today immer 0 Schritte, ein Fehler im Messskript. Behoben in `ecb6b739`.
- **Prüfungen:** acht Bauten ohne Warnung, Host 409/409, alle Werkzeuge grün, drei QEMU-Schlussläufe
  grün.
- **Mutationen:** 43 von 43 rot (14 auf die Kriterien, 5 auf Schritt 1 und 2, 24 auf die Befunde der
  Gegenprüfung), dazu die QEMU-Mutation, rot an L1 und am Kernel-Heap.
- **Offen bis zur ersten Vorabversion:** Abschnitt 7, dazu Entscheid F17.

## 1. Kriterien von Schritt 3 (Spezifikation Abschnitt 7, Entscheid F15)

| Nr. | Kriterium | Stand | Belege |
|---|---|---|---|
| 1 | Pebble-Code nur P7, K1-Wächter grün | **erfüllt** | `k1_waechter.py` grün; P7 = 4 Zeilen in `fw/apps/system/CMakeLists.txt`; Mutationen K1a, K1b rot |
| 2 | Ohne Systemrechte: keine beschreibbaren `static`, nur erlaubte Aufrufe, rote Liste leer; die Fehlerprobe beendet nur die App | **erfüllt** | `pruefe_ohne_rechte.py` auf obelix, obelix Release, emery grün, im Prüfbau nur die Fehlerprobe rot; seit A9 nur echte Syscalls aus dem Abbild; QEMU: Fehlerprobe endet am DACCVIOL, die Uhr läuft weiter; Mutationen K2a, K2b rot. Dazu an der Grenze: K1 (S3-Puffer) und K2 (Ausrichtung) behoben |
| 3 | Grundsatz: jede Zahl von der Uhr; nichts vom Telefon ausser den Zielen; kein Koffein, keine Getränke, keine Präparate | **erfüllt** | `test_bh_grundsatz` (8 Fälle); Mutationen K3a, K3b rot |
| 4 | Gleiche Urteile wie Boulder am selben Morgen (Schlaf, Ruhepuls, HRV, Ziele), Orakel 0 Abweichungen | **erfüllt** (Host) | `test_bh_rechnung` gegen Boulders Kotlin (Orakel aus AP5); seit C2/C4 dieselbe Nacht des Tages wie Boulder (`test_bh_daten`); Mutationen K4a, K4b rot |
| 5 | Einstiege landen auf der richtigen Karte; andere Boards behalten Pebbles App | **erfüllt** | `test_bh_einstieg`, `test_bh_einstiege` (Pebbles echter Code), `test_bh_startmenue`, `test_bh_startmenue_asterix`; asterix, getafix, flint, gabbro linken Pebbles `health/` (13 Objekte), die Boulder-Bauten keins; Mutationen K5a, K5b rot |
| 6 | Ladezeit L1–L6 in QEMU; auf der Uhr im Gerätetest | **teilweise**: QEMU erfüllt, Uhr offen. *Nachtrag 21.10.: Seit F40 begrenzt L2 den Anteil der App; wann das Neue zu sehen ist, bestimmen Pebbles Animationen (Abschnitt 19)* | Drei QEMU-Schlussläufe grün (Abschnitt 3). Der erste Start nach dem Boot liegt nahe an der Grenze und schwankt in Echtzeit-QEMU mit dem Host; unter `-icount` 17,6 ms wie mit dem Code von AP8. QEMU-Mutation (60 ms vor dem ersten Bild) an L1 rot (89,8 ms); Mutation K6b (Taste liest, L3) rot |
| 7 | Kein Kernel-Heap-Verlust nach 100 Starts, kein Puls-Abo nach dem Ende, kein Neustart | **erfüllt** (QEMU) | QEMU-Schlussläufe a–c; QEMU-Mutation (16 B Kernel-Heap je S2) am Kernel-Heap rot (49 372 → 48 304 → 47 304 B); Mutation K7a rot; seit A2 läuft Puls live auch nach einem abnormalen Ende höchstens 2 min |
| 8 | `boulder_ziele` vom Telefon gilt auf Uhr und in Boulder; ohne Telefon der letzte Wert oder die Vorgabe | **teilweise**: Uhr erfüllt, Boulder offen. *Nachtrag 13.10.: Boulder 0.53 erfüllt es in den Tests (Abschnitt 11), Release und Gerätetest offen* | `test_boulder_schnittstelle` (Einfügen, Ändern, Löschen, Leeren, ungültige Sätze, Neustart); Mutationen K8a, K8b rot; die Seite „Goals“ in Boulder 0.53 fehlt (Schlüssel ohne Nullbyte, K4) |

## 2. Gegenprüfung (12.10.2026)

Drei Prüfer, nur lesend, je ein Blickwinkel: Kern und Syscalls, App ohne Systemrechte, Inhalt und
Einstiege gegen Spezifikation und Boulder. Jeden Befund habe ich am Code nachgeprüft; alle 23 trafen
zu (21 verschiedene, zwei doppelt gemeldet). Jede Behebung hat einen Test, der ohne sie rot ist
(Abschnitt 4, Gruppe G).

| Nr. | Schwere | Befund | Behoben | Test |
|---|---|---|---|---|
| K1 | hoch (Sicherheit) | `sys_boulder_spo2`: Für `max = UINT32_MAX/8` lief das letzte Byte der Pufferprüfung in 32 Bit auf `buf − 9` um; ein Puffer am Ende des App-RAMs ging für bis zu 96 Werte durch – bis 760 B Schreiben mit Systemrechten hinter das App-RAM | `11f0bb2a`: erst `MIN(max, 96)`, dann prüfen | `test_boulder_schnittstelle`: Attrappe rechnet wie die Uhr in 32 Bit; ohne Fix Absturz |
| K2 | mittel | S2, S3, S7 kopierten per Strukturzuweisung (STM/STRD); ein schiefer Puffer einer App → UsageFault mit Systemrechten → Neustart | `11f0bb2a`: `memcpy` | objdump obelix Release: Mehrwort-Speichern auf den App-Puffer vorher 2 + 1 + 5 Speicherbefehle (S2, S3, S7), nachher 0 (`ap9/k2/`); auf dem Host nicht prüfbar, Mutation G-K2 am Abbild |
| K3 | mittel | „Laden“ nur einmal eingereiht; war die Schlange beim ersten init knapp, blieb die Variante bis zum Neustart ohne Nacht, Nächte, Herz, Woche | `11f0bb2a`: der Minutenteil reiht es nach | `test_boulder_schnittstelle__laden_bei_knapper_schlange_holt_die_minute_nach` |
| K4 | niedrig | Ziele: Filter nahm den Schlüssel auch mit Nullbyte an, gelesen wurde nur ohne | `11f0bb2a`: nur ohne Nullbyte (Vertrag für Boulder 0.53) | `andere_schluessel_loesen_nichts_aus` prüft jetzt den Wert |
| K5 | niedrig | Herzband: Uhr > 24 h zurück → Band und Zonen in der Zukunft, keine Minute zählte bis zum Einholen | `11f0bb2a`: der Takt stösst das Nachrechnen an | `test_boulder_herz__uhr_weit_zurueck_rechnet_nach` |
| K6 | niedrig | Eine wieder wachsende fertige Nacht las die offene App nicht neu | `022dd797`: S1/S5 je Minute, solange `NachtFertig` | `test_bh_daten__auffrischen_liest_die_fertige_nacht` |
| A1 | mittel | Use-after-free beim Schliessen eines Details (ScrollLayer vor ihrem Kind frei) | `082171a9`: Kind zuerst | Attrappe kennt das Kind der ScrollLayer |
| A2 | mittel | Puls live ohne Ablauf im Kern: nach abnormalem Ende bis 1 h Sekundentakt | `082171a9`: Ablauf 120 s (`sys_hrm_manager_set_update_interval`) | `test_bh_app__puls_live` |
| A4 | mittel | Tests fingen Kernaussagen nicht (Select/Precision lesen neu, Takt 5 s) | `082171a9`: Tests ergänzt | `alle_tasten_ohne_lesen`, `puls_live` |
| A5 | niedrig | Detailhöhe beim Öffnen fest; kam die Nacht mit Phase 2, sah man nur 60 px | `082171a9`: Höhe nach neuen Daten | `detail_waechst_mit_den_daten` |
| A6 = C7 | niedrig | Tracking aus: Details und Puls live liefen trotzdem | `082171a9`: kein Detail ausser Precision, kein Puls live | `test_bh_app__tracking_aus` |
| A7 | niedrig | Puls live lief im Heart-Detail weiter | `082171a9`: nur auf der Karte, zurück per `appear` | `puls_nur_auf_der_karte` |
| A8 | niedrig | Today urteilte über eine unfertige Nacht | `022dd797`: „so far“ wie Night | `today_ohne_urteil_fuer_unfertige_nacht` |
| A9 | niedrig | `pruefe_ohne_rechte.py` liess jedes `sys_*` durch (2 von 191 sind Kernfunktionen) | `13e7a021`: Syscalls aus `pebbleos.elf` | `test_pruefe_ohne_rechte.py`, 4 neue Proben |
| C1 = A3 | hoch | Night: Dauer und Urteil überlagert („6:30 h“ + „Very short night“, jede Nacht unter 7 h); Passtest aus 2.5 fehlte | `022dd797`: Urteil rechts der Einheit, umbrochen, notfalls GOTHIC 14 | `urteile_passen_neben_die_zahl` + 3 Bilder |
| C2 | mittel | HRV-Urteil fehlte bis 2 h nach dem Aufwachen (S5 erst bei „endgültig“) | `022dd797`: S1 zählt für ihren Wachtag schon ab „fertig“ | `nachttage_mit_der_fertigen_nacht` |
| C3 | mittel | Heart: Ruhepuls/HRV aus S1 ohne Wachtag (alte Werte) | `022dd797`: wie Today aus dem heutigen Wachtag | `heart_ohne_alte_nacht`, `heart_und_today_gleich` |
| C4 | mittel, selten | Folgenacht: Today zeigte die kürzere Nacht | `022dd797`: längste Nacht des Tages (jeTag) | `folgenacht_die_laengste_gilt` |
| C5 | niedrig | Uhrzeiten in der Nacht der Zeitumstellung 1 h falsch | `022dd797`: Zone im Zeitpunkt (`bh_ortsversatz`) | `uhrzeit_in_der_nacht_der_umstellung` |
| C6 | niedrig | Night-Detail: SpO2 nur aus 24 h | `022dd797`: ab Nachtbeginn | `spo2_ab_dem_beginn_der_nacht` |
| C8 | niedrig | Zonenminuten ohne Tausendertrenner | `022dd797`: `bh_zahl` | `zonen_mit_tausendern` |

## 3. Prüfungen

### Bauten
Alle acht Bauten liefen ohne Warnung und ohne Fehler: obelix@pvt, obelix@pvt Release, qemu_emery,
asterix, getafix@dvt2, qemu_flint, qemu_gabbro und der Prüfbau (qemu_emery mit
`CONFIG_BOULDER_PRUEFLAUF` und `CONFIG_BOULDER_HEALTH_ZEITEN`). Gebaut wurde in drei Stufen: sauber neu
vor der Gegenprüfung, inkrementell nach jeder Änderung und im Schlusslauf auf `ecb6b739`. Im Schlusslauf
wurden alle Boulder-Quellen neu übersetzt (App, Kieselsport, Kern), 66 bis 541 Dateien je Bau.
SDK 0.1.10, clang-format 22. Belege: `bauten.txt`, `schluss_pruefen.sh`.

### Host
Voller Lauf (`pbl test`) auf `ecb6b739`: **409 von 409 grün**. In der Cloud lag die Grundlinie bei 404.
Hinzugekommen sind 3 Tests aus AP6 und 2 aus dieser Gegenprüfung (`test_bh_unbenutzt`,
`test_pruefe_unbenutzt_gegenproben`).

### Werkzeuge (Schlusslauf auf `ecb6b739`, `werkzeuge/`)
| Werkzeug | Ergebnis |
|---|---|
| `k1_waechter.py` | grün (Kriterium 1) |
| `kopie_pruefen.py`, `test_kopie_pruefen.py` | grün |
| `vertraege.py` | grün |
| `umzug_pruefen.py`, auch mit `--gegenprobe` | grün |
| `prueflauf_daten.py` | grün |
| `pruefe_unbenutzt.py` für die Health-App (jetzt mit `TEXT()`-Tabellen) und Kieselsport | grün |
| `test_pruefe_unbenutzt.py` (neu, 6 Proben), `test_pruefe_ohne_rechte.py` (jetzt 24 Proben) | grün |
| `pruefe_ohne_rechte.py` obelix, obelix Release, emery | grün |
| `pruefe_ohne_rechte.py` Prüfbau | rot nur an der Fehlerprobe (gewollt) |
| `k2/mehrwort.sh` obelix Release | grün: kein STM/STRD in den Kopierwegen von S1–S7 |

### QEMU (Prüfbau, `tools/boulder/qemu_health.py`)

Drei Schlussläufe in Echtzeit-QEMU, wie in 2.6.3 festgelegt (`belege-schritt3/ap9/qemu-a/`, `qemu-b/`,
`qemu-c/`). Die Läufe a und b liefen auf `55d8e4c9`, Lauf c auf `ecb6b739` mit derselben Firmware und dem
korrigierten Messskript (siehe unten, Schritte). Angegeben ist der Höchstwert, in Klammern der Median:

| Ziel | Lauf a | Lauf b | Lauf c | Grenze |
|---|---|---|---|---|
| L1 erster Start nach dem Boot | 49,8 ms | 33,2 ms | 40,0 ms | 50 ms |
| L1 die übrigen 59 Starts (Startmenü, Pin Night, Pin Activity) | 11,7 (6,8) | 16,6 (6,8) | 27,3 (6,8) | 50 ms |
| L2 Karte | 13,7 (5,9) | 14,6 (6,4) | 11,7 (5,9) | 30 ms |
| L2 Detail | 12,7 (6,4) | 17,6 (7,3) | 18,6 (6,4) | 30 ms |
| L4 Auffrischen im Minutentakt | 8,8 (4,9) | 8,8 (6,4) | 6,8 (5,9) | 30 ms |
| L6 je Syscall (neun) | 0,01 | 0,01 | 0,01 | 1 ms |

Prüfungen, in allen drei Läufen grün:
- Puls im Sekundentakt nur auf Heart, keiner nach dem Verlassen der Karte und keiner nach dem Ende.
- Kernel-Heap ohne Wachstum über 100 Starts (Lauf c: 52 744 → 52 676 → 52 676 B; einmalig 68 B wie in AP8).
- App-Heap höchstens 5 404 B.
- Syscall-Stapel: 1 352–1 368 B Reserve, nie benutzt.
- Die Fehlerprobe endet am DACCVIOL, die Uhr läuft weiter.
- Kein Neustart im ganzen Lauf.

**Erster Start nach dem Boot.** Er kam als einziger Start nahe an die Grenze und schwankte in AP9
zwischen 30 und 54 ms. Ich habe ihn deshalb untersucht (`belege-schritt3/ap9/kaltstart/`):

- In Reihen mit getauschtem App-Code schwankt er mit dem Code von AP8 ebenso (30,3–38,1 ms gegen
  31,2–53,7 ms). Die schnellen Läufe liegen bei beiden um 31 ms.
- Die Rechenzeit des Hosts, gemessen vor und nach dem Start, erklärt es nicht. Der Zeitraffer der
  Prüfnacht ebenso wenig, denn er ist nach 2 s fertig.
- Mit `-icount` zählt QEMU die Gastzeit nach ausgeführten Befehlen. So gemessen braucht der erste Start
  17,6 ms, mit dem Code von AP8 16,6–17,6 ms, und jeder weitere Start 8,8 ms.

Beim ersten Start tut die App also gleich viel wie in AP8. Was in Echtzeit-QEMU dazukommt und schwankt,
ist QEMUs Übersetzen jedes Codepfads beim ersten Lauf, und das gibt es auf der Uhr nicht. Ein Vorschlag
für die Messung steht in Abschnitt 7 (F17).

**Schritte.** Bis AP9 zeigte Today in allen QEMU-Bildern „Steps 0“, auch schon in AP8. Die Ursache:
Pebble beginnt an der ersten Minutengrenze nach dem Stellen der Uhr einen neuen Tag und schob dabei die
kurz vorher gesetzten 6 230 Schritte auf gestern. Seit `ecb6b739` setzt das Messskript die Schritte erst
nach diesem Wechsel; in Lauf c zeigen Today und Activity 6 230.

**QEMU-Mutation** (`qemu-mutation/`): 60 ms Warten vor dem ersten Bild und 16 B Kernel-Heap je
`sys_boulder_lage`, die nie frei werden. Ergebnis: rot an L1 (89,8 ms) und am Kernel-Heap
(49 372 → 48 304 → 47 304 B). Danach waren die Quellen zurück und der Prüfbau neu gebaut.

## 4. Mutationen

Jede Mutation baut einen Fehler in eine Datei ein, und die genannte Prüfung muss dann rot werden.
Danach kommt die Datei aus dem Speicher zurück (nicht aus git), und die Bauordner werden neu gebaut.
Skript und Ausgabe: `belege-schritt3/ap9/mutationen.py`, `mutationen.txt`.

Ergebnis auf dem Endstand `ecb6b739`: **43 von 43 rot**. Dazu kommt die QEMU-Mutation für K6 (L1) und
K7 (Kernel-Heap), rot an beiden (Abschnitt 3).

Was der Vorlauf (`mutationen_vorlauf.txt`) zeigte:
- Zwei Mutationen zeigten noch auf Code von vor der Gegenprüfung. K2b baute nicht, und K8b fand ihre
  Stelle nicht. Beide sind jetzt an den heutigen Code angepasst.
- S1-3 mit 30 statt 40 min blieb grün. Das ist ein gleichwertiger Mutant: Von 25 bis 60 min rechnen
  alle Prüfnächte gleich (Kommentar in `schlafanalyse.c`), also kann kein Test ihn sehen. S1-3 setzt
  jetzt den Wert von b25, 20 min, der am 5.10. 29 Minuten zu früh einschlafen liess, und wird rot.

**Gruppe K: Kriterien von Schritt 3**

| Nr. | Fehler | Datei | rot in |
|---|---|---|---|
| K1a | Pebble-Datei außerhalb der Liste geändert | `health.c` | `k1_waechter.py` |
| K1b | P7 bringt mehr als den Filter | `CMakeLists.txt` | `k1_waechter.py` |
| K2a | beschreibbares static in der App | `daten.c` | `pruefe_ohne_rechte.py` (obelix) |
| K2b | Aufruf aus der roten Liste (Minutenverlauf, L5) | `daten.c` | `pruefe_ohne_rechte.py` (obelix) |
| K3a | Zonen ohne Quelle S6 gezeigt | `details.c` | `test_bh_grundsatz` |
| K3b | Text vom Telefon (Koffein in mg) | `texte_tabelle.h` | `test_bh_grundsatz` |
| K4a | Schlafurteil: Grenze "kurz" 45 statt 60 min | `rechnung.c` | `test_bh_rechnung` |
| K4b | Ruhepuls "as usual" mit 3 statt 2 bpm Spanne | `rechnung.c` | `test_bh_rechnung` |
| K5a | Quick Launch Oben gespiegelt statt Unten | `einstieg.c` | `test_bh_einstieg`, `test_bh_einstiege` |
| K5b | Pebbles Health-App nur noch mit dem Schalter (asterix ohne) | `system_app_registry_list.json` | `test_bh_startmenue_asterix` |
| K6b | Taste liest Daten (L3) | `boulder_health.c` | `test_bh_app` |
| K7a | Puls-Takt bleibt nach dem Ende | `boulder_health.c` | `test_bh_app` |
| K8a | Leeren der Datenbank Health lässt alte Ziele stehen | `ziele.c` | `test_boulder_schnittstelle` |
| K8b | Schlafurteil mit festem Ziel statt dem aus der Lage | `inhalt.c` | `test_bh_karten_obelix` |

**Gruppe S: Kernaussagen von Schritt 1 und 2**

| Nr. | Fehler | Datei | rot in |
|---|---|---|---|
| S1-3 | Wälzregel beim Einschlafen wie b25: 20 statt 40 min | `schlafanalyse.c` | `test_boulder_schlafkern`, `test_boulder_naechte` |
| S1-4 | Smart Alarm ohne Bewegung (auch im Tiefschlaf) | `wecker.c` | `test_boulder_wecker`, `test_boulder_wecker_vergleich` |
| S1-5 | Velo nie erkannt | `kontext.c` | `test_boulder_fahrt` |
| S2-1 | Velo-Training zählt Schritte | `plattform.c` | `test_kieselsport`, `test_kieselsport_folgen` |
| S2-5 | Pulskalorien: Ruheanteil 3,0 statt 3,5 ml/kg/min | `kalorien.c` | `test_boulder_kalorien`, `test_kieselsport_kalorien` |

**Gruppe G: Befunde der Gegenprüfung**

| Nr. | Fehler | Datei | rot in |
|---|---|---|---|
| G-A1 | Inhaltsebene nach ihrer ScrollLayer frei (A1) | `boulder_health.c` | `test_bh_app` |
| G-A2 | Puls-Abo ohne Ablauf im Kern (A2) | `boulder_health.c` | `test_bh_app` |
| G-A4a | Select liest neu (A4 a) | `boulder_health.c` | `test_bh_app` |
| G-A4b | Precision liest beim Setzen neu (A4 b) | `boulder_health.c` | `test_bh_app` |
| G-A4c | Puls live im 5-s-Takt (A4 c) | `boulder_health.c` | `test_bh_app` |
| G-A5 | Detailhöhe nach neuen Daten nicht neu (A5) | `boulder_health.c` | `test_bh_app` |
| G-A6 | Tracking aus öffnet Details (A6) | `boulder_health.c` | `test_bh_app` |
| G-A7 | Puls live auch im Detail (A7) | `boulder_health.c` | `test_bh_app` |
| G-C1 | Urteil nie umbrochen (C1) | `zeichnen.c` | `test_bh_karten_obelix` |
| G-C1b | Grenze ohne Layout zu eng (Very short night passt nicht) | `zeichnen.h` | `test_bh_karten_obelix` |
| G-C2 | S1 nicht in der Nächteliste (C2) | `nachttage.c` | `test_bh_daten` |
| G-C3 | Heart zeigt einen alten Wachtag (C3) | `inhalt.c` | `test_bh_grundsatz` |
| G-C4 | Folgenacht verdrängt die längere Nacht (C4) | `nachttage.c` | `test_bh_daten` |
| G-C5 | Uhrzeit mit dem Versatz des Fensterbeginns (C5) | `nachttage.c` | `test_bh_daten` |
| G-C6 | SpO2 nur aus 24 h (C6) | `daten.c` | `test_bh_daten` |
| G-C8 | Zonenminuten ohne Tausender (C8) | `details.c` | `test_bh_grundsatz` |
| G-A8 | Today urteilt über eine unfertige Nacht (A8) | `inhalt.c` | `test_bh_grundsatz` |
| G-K6 | Fertige Nacht im Minutentakt nicht neu gelesen (K6) | `daten.c` | `test_bh_daten` |
| G-K1 | S3 prüft den Puffer mit max statt dem Begrenzten (K1) | `schnittstelle.c` | `test_boulder_schnittstelle` |
| G-K2 | S2 kopiert per Strukturzuweisung (K2) | `schnittstelle.c` | `mehrwort.sh` (obelix Release) |
| G-K3 | Laden nicht nachgeholt (K3) | `dauerteil.c` | `test_boulder_schnittstelle` |
| G-K4 | Ziele-Filter nimmt auch längere Schlüssel (K4) | `ziele.c` | `test_boulder_schnittstelle` |
| G-K5 | Uhr zurück: kein Nachrechnen (K5) | `herz.c` | `test_boulder_herz` |
| G-A9 | `pruefe_ohne_rechte` lässt jedes `sys_*` durch (A9) | `pruefe_ohne_rechte.py` | `test_pruefe_ohne_rechte.py` |

## 5. Schritt 1 und 2 erneut (auf dem Stand nach der Gegenprüfung)

Alle Tests laufen im vollen Host-Lauf mit (409/409). Dazu je eine Mutation auf eine Kernaussage
(Abschnitt 4, Gruppe S).

| Schritt | Kriterium | Prüfung | Stand |
|---|---|---|---|
| 1 | 1 Pebble-Code nur an den erlaubten Stellen | `k1_waechter.py`, `test_boulder_k1_waechter`, `test_boulder_waechter_gegenproben` | grün |
| 1 | 2 Schritte wie das Original | `test_boulder_kraepelin_gleich`, `test_boulder_glue_differenz*`, `kopie_pruefen.py` | grün |
| 1 | 3 Schlaf auf den Testnächten | `test_boulder_naechte`, `test_boulder_schlafkern`, `test_boulder_schlafanalyse_wie_b26` | grün; Mutation S1-3 rot |
| 1 | 4 Live, Smart Alarm, Nickerchen | `test_boulder_zyklen`, `test_boulder_wecker`, `test_boulder_wecker_vergleich` | grün; Mutation S1-4 rot |
| 1 | 5 Velo/Auto | `test_boulder_fahrt` | grün; Mutation S1-5 rot |
| 1 | 6 HRV, SpO2, Messstufe, 0x4001/0x4002 | `test_boulder_messung`, `test_boulder_vertraege*`, `vertraege.py` | grün |
| 1 | 7 Minutenteil ohne Heap, ≤ 100 ms | `test_boulder_messung`, QEMU-Prüfbau (`prueflauf_daten.py`) | grün in QEMU; 7 Nächte auf der Uhr offen (Gerätetest) |
| 1 | 8 Wechsel von b25 | `umzug_pruefen.py` (mit Gegenprobe), `test_boulder_umzug_*` | grün |
| 2 | Kieselsport (Dienst, App, Kalorien, Schritte) | `test_kieselsport*`, `test_boulder_kalorien`, `test_workout_service_kieselsport` | grün; Mutationen S2-1, S2-5 rot |
| 2 | tote Texte und Funktionen | `test_boulder_apps_unbenutzt` | grün |

*Nachtrag 13.10.:* `test_boulder_schlafanalyse_wie_b26` ist seit `4ea6fb64` ersetzt durch
`test_boulder_schlaf`, weil die Schlafrechnung von V2 jetzt von b26 abweicht (Nachtrag in der
Spezifikation). Mutation S1-3 setzt seither 30 auf 20 und ist auf `4ea6fb64` rot
(`naechte/analyse-2026-10-13.md`, Abschnitt 7).

## 6. Bekannte Grenzen (bleiben so, im Gerätetest ansehen)

- **Boot am Kabel:** Den Takt plant erst „Laden“, und das läuft erst mit Pebbles Tracking-Teil. Startet die
  Uhr am Kabel, liefert sie bis zum Abstecken keine Lage (Ziele = Vorgabe) und keine Nächte, kein Herz,
  keine Woche (S2, S5–S7). Danach ist alles da. Steht in `schnittstelle.h`.
- **Night nach einer Folgenacht:** Today zeigt wie Boulder die längste Nacht des Tages; Night zeigt den
  Block der zuletzt gerechneten Nacht (S1), also die Folgenacht. Den Block der längeren hat die Uhr dann
  nicht mehr.
- **Zeitumstellung:** Pebble kennt nur die Sommerzeit des laufenden Jahres. Nächte über einen Jahreswechsel
  der Regeln hinweg sind nicht geprüft.
- **Mehrwort-Speichern (K2):** Auf dem Host nicht prüfbar (x86 erlaubt schiefe Zugriffe, der Testbau hat
  keine Schalter je Test). Belegt am Abbild (objdump); bei einer neuen Compiler-Fassung wiederholen.
- **Erster Start in Echtzeit-QEMU:** Er enthält QEMUs Übersetzen jedes Codepfads beim ersten Lauf und
  schwankt deshalb mit dem Host (30–54 ms bei 50 ms Grenze). Ein Lauf kann an L1 rot werden, ohne dass
  sich die App geändert hat (Abschnitt 3; Vorschlag F17).

## 7. Bis zur ersten Vorabversion (Spezifikation Abschnitt 6, ergänzt)

1. Upstream v4.40.0 nachziehen (`kopie_nachziehen` war rot); danach die Probe ohne Systemrechte, die
   QEMU-Messung und `pruefe_ohne_rechte` wiederholen (MPU und Syscalls umgebaut).
   *Erledigt am 13.10. (`0510648c`), siehe Abschnitt 8; dazu v4.41.0 (F18), Abschnitt 9.*
2. Wechsel b26 → V2 testen (bisher mit einem b25-Abbild).
   *Erledigt am 13.10. (`b6a4b3df`), siehe Abschnitt 10.*
3. Boulder 0.53: Seite „Goals“ schreibt `boulder_ziele` (8 B, **Schlüssel ohne Nullbyte**, Befund K4),
   V2 als Firmware erkennen, ~~Gravel 0.3.4~~ (gestrichen: Boulder baut nicht mehr auf Gravel auf,
   `entscheidungen.md`).
   *Gebaut am 13.10. im Arbeitszweig von Boulder, siehe Abschnitt 11. Das Release steht aus.*
4. Kieselstrasse nach V2 portieren (F6).
   *Erledigt am 14.10. (`d4e15214`), siehe Abschnitt 12; Vorabversion b28.*
5. ~~Vorher-Video von b26 (braucht die Uhr).~~ *Gestrichen am 14.10. (Nutzer): kein Video, die
   Einschätzung der Ladezeiten auf der Uhr durch den Nutzer genügt.*
6. Gerätetest: 7 Nächte, Fitbit-Nächte, Velo- und Autofahrten, Messstufen, Ladezeiten auf der Uhr
   (Kriterium 6, nach der Einschätzung des Nutzers), dazu die Grenzen oben.
7. Ruff: 45 ältere Befunde in den Boulder-Werkzeugen (`tools/boulder/`, zwei Testhelfer). Upstreams
   Python ist sauber, und die CI von PebbleOS prüft Ruff bei Pull Requests.

### Entschieden am 13.10.: F17 B, F18 A

**F17 Erster Start in QEMU.** In Echtzeit-QEMU zählt beim ersten Start nach dem Boot QEMUs Übersetzen
mit. Dieser Anteil schwankt mit dem Host, darum kam der Start in AP9 auf 30–54 ms. Die übrigen 59 Starts
lagen in den Schlussläufen bei höchstens 27,3 ms.
- A: so lassen (Höchstwert über alle Starts in Echtzeit). Ein Lauf kann dann rot werden, ohne dass sich
  etwas geändert hat.
- B: `qemu_health.py --icount`: Ein zweiter Boot mit `-icount shift=3` misst den ersten Start nach dem
  Boot. Dort zählt nur, was die App ausführt (heute 17,6 ms). In Echtzeit zählen alle übrigen Starts wie
  bisher.
- **Empfehlung: B.** Es misst die Bauart (2.6.3) statt des Emulators, und das Ergebnis ist wiederholbar.
- **Entschieden: B** (13.10.), umgesetzt in `f2753ba4`. Belege: `belege-vorab/f17/`.
- *Nachtrag 13.10. (v4.40.0):* Wieder dasselbe Bild. In Echtzeit kam der erste Start auf 39,1 und
  52,7 ms, ein Lauf wurde an L1 rot. Unter `-icount` waren es 16,6 und 17,6 ms wie in AP9, die übrigen
  59 Starts lagen bei höchstens 24,4 ms. Die App ist nicht langsamer geworden.

**F18 Upstream v4.41.0.** Seit dem 12.10. gibt es v4.41.0 (216 Commits nach v4.40.0).
- **Inhalt:**
  - Der grösste Teil stellt den ganzen Baum auf C23 um: `nullptr`, `[[maybe_unused]]`,
    `unreachable()`, `static_assert`, sortierte Includes; Firmware und Tests bauen als C23.
  - Pulsdienst: Ein Ereignis aus einem schon beendeten Sensor-Abo konnte den Kern abstürzen lassen,
    wenn KernelBG den Sensor aus- und wieder einschaltet (`21a43337`). Das tut Boulders Messstufe.
  - Wecker: Ein Schlummern endet, wenn der Plan des Weckers geändert wird (`62e8d0a8`), in `alarm.c`
    neben Boulders Weckregel.
  - Beschleunigungssensor: Apps, die die Batch-Grösse ändern, bekamen keine Daten mehr oder wurden
    beendet (`1c85cc53`, `c8a812ea`). Dazu Korrekturen an Bluetooth.
- **Aufwand:**
  - Ein Probe-Merge ergab vier Konfliktstellen in drei Dateien (`settings/health.c`,
    `workout_service.c`, `alarm.c`).
  - Die Kraepelin-Originale ändern sich nur im Stil (66 Zeilen); die Kopie zieht das mit
    `git merge-file` nach.
  - `PBL_UNUSED` und `PBL_NORETURN` fallen weg, Boulder nutzt sie in je einer Datei.
- A: v4.41.0 jetzt nachziehen, dann den Stand bis nach dem Gerätetest einfrieren. Der Gerätetest prüft
  dann den Code, der bleibt.
- B: Die erste Vorabversion auf v4.40.0, v4.41.0 erst nach dem Gerätetest. Das geht schneller an die
  Uhr, aber der Gerätetest prüft eine Basis, die danach wechselt.
- **Empfehlung: A.** Die Korrektur im Pulsdienst betrifft genau, was der Gerätetest prüft (7 Nächte
  ohne Absturz), und der Aufwand ist überschaubar.
- **Entschieden: A** (13.10.). Nachgezogen in Abschnitt 9; der Upstream-Stand bleibt bis nach dem
  Gerätetest eingefroren.

## 8. Upstream v4.40.0 (13.10.2026, `0510648c`)

Punkt 1 aus Abschnitt 7. Belege: `belege-vorab/upstream-v4.40.0/`.

- **Merge:** Tag v4.40.0 (131 Commits seit v4.39.0), ohne Konflikt.
  - Upstream ändert zwei Dateien, in denen Boulder Stellen hat: `alarm.c` (gespeicherte Weckzeit
    `int32_t` statt `time_t`) und `settings_blob_db.c` (Callback als eigene Funktion). Boulders Stellen
    bleiben unverändert.
  - Die Kraepelin-Originale sind unverändert. `HERKUNFT` und die Köpfe der Kopie nennen jetzt v4.40.0,
    `kopie_pruefen.py --nachziehen v4.40.0` ist grün.
- **Nachgezogen im Merge-Commit:**
  - Testpfade: tinflate und uPNG liegen jetzt in `third_party` (uzlib, upng), `flash_crc.c` in
    `drivers/`.
  - `umzug_pruefen.py`: `ActivitySession.start_utc` ist upstream `int32_t` statt `time_t`. Auf der Uhr
    ist `time_t` 32 bit (beim Bauen zugesichert), b25s Sitzungen liegen also gleich. Eine neue
    Gegenprobe zeigt, dass ein anders breiter Beginn rot bleibt.
- **Ohne Systemrechte:**
  - Upstream prüft jetzt beim Bauen, dass Privilegienprüfungen nur in Syscall-Rümpfen stehen
    (`c3610b6e`). Das ist auf allen acht Bauten grün, auch für S1–S7.
  - `pruefe_ohne_rechte.py` ist auf obelix, obelix Release und emery grün; im Prüfbau ist nur die
    Fehlerprobe rot, wie gewollt.
  - QEMU mit Upstreams neuem MPU-Code: Die Fehlerprobe endet am DACCVIOL, die Uhr läuft weiter.
- **Prüfung:**
  - Acht Bauten sauber ohne Warnung.
  - Host 433/433: Upstream bringt 24 Tests mit.
  - Alle Werkzeuge grün; K1 misst jetzt gegen v4.40.0.
  - Mutationen 43/43 rot.
- **QEMU, zwei Läufe:**
  - Lauf a ist grün.
  - Lauf b ist an L1 rot: 52,7 ms am ersten Start nach dem Boot. Das ist F17; unter `-icount` liegt
    der erste Start bei 17,6 ms wie vorher.
  - Alle übrigen Werte sind grün: L2 höchstens 20,5 ms, L4 7,8 ms, L6 0,01 ms. Der Kernel-Heap wächst
    nicht, der Syscall-Stapel hat 1 368 B Reserve, und es gab keinen Neustart.
- **Offen:** Upstream ist schon bei v4.41.0 (F18). Gegen `upstream/main` ist `kopie_nachziehen` deshalb
  wieder rot.

## 9. Upstream v4.41.0 (13.10.2026, `273728a3`)

F18 A. Belege: `belege-vorab/upstream-v4.41.0/`. **Der Upstream-Stand bleibt jetzt bis nach dem
Gerätetest eingefroren.**

- **Merge:** Tag v4.41.0 (216 Commits nach v4.40.0). Drei Konflikte in Pebble-Dateien mit Boulder-Stellen,
  alle neben Boulders Blöcken: sortierte Includes und `NULL` → `nullptr`. Boulders Stellen sind
  inhaltlich gleich geblieben.
- **Kraepelin-Kopie:** Upstreams Stiländerungen an den Originalen kamen per `git merge-file` in die
  Kopie. Die Includes der Blöcke D0, D2 und D10 stehen jetzt in Upstreams sortierter Liste.
  `HERKUNFT` nennt v4.41.0 mit den neuen SHA-256.
- **C23 und `-Werror` im Host-Bau:**
  - `PBL_UNUSED` und `PBL_NORETURN` gibt es nicht mehr; Boulder nutzt jetzt `[[maybe_unused]]` und
    `[[noreturn]]`.
  - `kontext.c`: `prv_kcal_t` gibt es nur noch mit `CONFIG_BOULDER_KIESELSPORT`, wie ihre Aufrufer.
  - In den Tests sind nie gerufene Helfer aus `test_activity.c` entfernt, `wtf()` ist `[[noreturn]]`.
    Upstream hat `stubs_accel_manager.h` gelöscht, Boulders Tests haben dafür einen eigenen Stub mit
    den neuen Signaturen.
- **Wecker, geändertes Verhalten (Upstream `62e8d0a8`):** Jede Änderung an Zeit, Art oder Tagen beendet
  den laufenden Smart Alarm. Wer im Fenster die Weckzeit verstellt und wieder zurückstellt, wird an
  diesem Morgen nicht mehr geweckt, mit und ohne Neustart. Boulders Weckregel folgt dem. Der
  Weckertest erwartet das jetzt so und kennt den neuen Zustand `s_snooze_generation` von `alarm.c`.
- **Prüfung:**
  - Acht Bauten sauber ohne Warnung, die Firmware übersetzt als C23.
  - Host 437/437; Upstream bringt 4 neue Tests mit.
  - Alle Werkzeuge grün; K1 misst jetzt gegen v4.41.0. Upstreams Syscall-Prüfung ist grün.
  - Mutationen 43/43 rot. G-A2 musste umgeformt werden, weil ein Komma-Ausdruck ohne Wirkung unter
    `-Werror` nicht mehr baut; danach rot.
- **QEMU mit F17 B, zwei Läufe grün:**
  - L1 höchstens 13,7 ms.
  - Erster Start unter `-icount`: 16,6 und 17,6 ms. In Echtzeit waren es 50,8 und 51,8 ms; nach der
    alten Regel wären beide Läufe rot gewesen.
  - L2 15,6 ms, L4 6,8 ms, L6 0,01 ms.
  - Der Kernel-Heap wächst nicht, der Syscall-Stapel hat 1 368 B Reserve, die Fehlerprobe beendet nur
    die App, und es gab keinen Neustart.
- **Für den Gerätetest (Punkt 6):**
  - das geänderte Verhalten beim Umstellen eines laufenden Smart Alarms;
  - der Pulsdienst beim Aus- und Einschalten des Sensors durch die Messstufe (Upstreams Korrektur
    `21a43337`).
- *Nachtrag, SDK 0.1.11:* v4.41.0 verlangt SDK 0.1.11 mit GCC 15.2 (`SDK_VERSION`). Die Prüfung oben
  lief noch mit 0.1.10 (GCC 14.2). Wiederholt mit 0.1.11, alle Bauordner neu angelegt:
  - Acht Bauten ohne Warnung, Host 437/437, alle Werkzeuge grün.
  - K2 (`mehrwort.sh`): kein Mehrwort-Speichern in den Kopierwegen von S1–S7.
  - Mutationen 43/43 rot.
  - QEMU zweimal grün. L1 höchstens 17,6 ms; der erste Start unter `-icount` brauchte 19,5 und
    20,5 ms, mit GCC 14 waren es 16,6–17,6 ms.
  - Die Werkzeuge nehmen das SDK jetzt aus `SDK_VERSION`. Der Sitzungs-Hook nennt GCC 15 nicht mehr
    als Hindernis (`10b0d9d8`).
  - Belege: `belege-vorab/sdk-0.1.11/`.

## 10. Wechsel b26 → V2 (13.10.2026, `b6a4b3df`)

Punkt 2 aus Abschnitt 7. Belege und Ablauf: `belege-vorab/wechsel-b26/`.

- **Bezug b26 statt b25:** b25 und b26 unterscheiden sich in `src/` nur in der Schlafrechnung.
  `umzug_pruefen.py` vergleicht jetzt beide mit V2 (`b6a4b3df`); alle 15 Definitionen sind gleich.
- **Echter Wechsel in QEMU:**
  - b26 (`main` `a7d8b16e1`) lief für `qemu_emery` und erzeugte Zustand über die Tasten: Health
    Tracking, Kilometer, Messstufe Detailed, zwei Wecker (einer smart), ein gespeichertes
    Kieselsport-Training, das noch aufs Telefon wartet.
  - Dann schrieb ich V2s System-Ressourcen ins selbe Flash, wie bei einem Update, und startete V2.
- **Ergebnis:**
  - Alles ist übernommen: Einstellungen, Messstufe 4, beide Wecker, die Sitzung, die drei
    Datalogging-Spools und das wartende Training.
  - Nichts ist doppelt. Der Umzug lief nur beim ersten Start. Der zweite Start war stabil.
  - Keine Fehler.
- **Für die Release-Notiz:**
  - Nach dem Wechsel zeigt die Uhr Pebbles Standard-Zifferblatt, weil TimeStyle Bedrock nicht mehr
    in der Firmware ist. TimeStyle aus dem Store installieren.
  - Die alte Kieselsport-Store-App einmal löschen, falls sie doppelt erscheint (V9).
- **Nicht in QEMU prüfbar:** Nächte und Movement-Sitzungen (keine Sensordaten). Dafür stehen die
  Host-Tests T8.1–T8.3 und der Gerätetest.

## 11. Boulder 0.53 (13.10.2026, Boulder-Zweig `claude/exciting-cerf-m8apu2`, `aecd3350`)

Punkt 3 aus Abschnitt 7. Belege: `belege-vorab/boulder-0.53/`. Gebaut und geprüft in der Cloud
(Android-SDK, NDK 28.2, Gradle 9.6.1; Maven Central über Googles Spiegel, weil es hier mit 429
antwortet).

- **Gravel 0.3.4: gestrichen.** Boulder baut nicht mehr auf Gravel auf (`entscheidungen.md`, 13.10.).
  Ich hatte es zuerst übernommen (`36fad7e2`); `aecd3350` nimmt es zurück. Für V2 war es nicht nötig:
  V2 schickt Minutendaten in Fassung 14, die Boulder seit b25 liest, und Sitzungen in Fassung 7.
- **V2 erkennen** (`2916e3f0`):
  - V2 ist eine Boulder-Fassung (`-bN`) auf Pebbles v4.39 oder neuer. 0.52 hätte V2 ab b24 für
    Bedrock-fähig gehalten.
  - Auf V2 gibt es jetzt keine Bedrock-Kachel und keine Einstellungen dafür. Zurück auf b26 geht die
    gespeicherte Bedrock-Einstellung einmal neu zur Uhr.
  - **Folge für die Vorabversion:** V2 braucht einen Tag `v4.41.0-bN` (N ab 27). Ohne `-bN` gilt V2 als
    unbekannte Firmware, und die Bedrock-Kachel steht mit dem Hinweis „Needs Boulder firmware b22 or
    newer“ da.
  - Für das spätere Stable-Release von V2: Zeile `Mindest-App: 0.53.0` in die Notizen. Eine
    Vorabversion bietet Boulder ohnehin nicht an.
- **Goals** (`fcd56ca8`):
  - Neue Seite unter Einstellungen > Health: Schritte 1 000–50 000 in 500er-Stufen, Schlaf 4:00–12:00 h
    in Viertelstunden. Jede Änderung gilt sofort.
  - Boulder schreibt `boulder_ziele` als Zeile der Datenbank Health: 8 B, Fassung 1, little endian,
    Schlüssel 13 B ohne Nullbyte. Die BlobDB-Synchronisation bringt sie zur Uhr. Nach einer neuen
    Kopplung leert das Telefon die Datenbank auf der Uhr und sendet alles neu, die Ziele mit.
  - Beim Start der App gehen gemerkte Ziele noch einmal in die Datenbank, falls sie neu ist. Derselbe
    Wert geht nicht erneut zur Uhr (Hash der Zeile).
  - Boulder misst überall an diesen Zielen. Das Schlafurteil folgt derselben Regel wie
    `bh_schlafurteil` auf der Uhr.
  - Auch die Vergleiche („What makes a difference“) folgen den Zielen, ihre Gruppen nennen die Zahl
    („After 8,000+ steps“, „Under 6:30 h“). Sonst stünde auf der Trendseite neben der Ziellinie bei
    8 000 eine Gruppe „10,000+“.
  - Ohne Einstellung gelten 10 000 Schritte und 8:00 h, wie bisher und wie auf der Uhr.
- **Fassung 0.53.0** (`23b335a9`, Notiz berichtigt in `aecd3350`). CLAUDE.md von Boulder hält den
  Entscheid gegen Gravel jetzt fest.
- **Prüfung** (Baum von `aecd3350`):
  - 2143 Tests, 2 wie bisher übersprungen, nach `clean` jede Testaufgabe mit `--rerun`. Der
    Release-Build ist grün (0.53.0, mit dem CI-Schlüssel signiert, nicht debuggable, ohne Firebase
    und GMS).
  - Gegenproben mit eingebauten Fehlern: V2 (1 und 2 Tests rot), Ziele (Widget, Tagessatz, Vergleich
    je 1).
  - Die Seite ist in Robolectric gezeichnet (`ziele.png`), mit einem Tipp auf Plus.
- **Nicht geprüft:** der Weg über Bluetooth zur Uhr (geprüft bis zur Datenbank Health; dass die Uhr
  den Satz liest, prüft `test_boulder_schnittstelle`), Telefon und Emulator.
- **Release:** Fassung 0.53.0 mit Notiz liegt im Arbeitszweig. Das Release legt die CI an, sobald der
  Stand auf `main` ist (Tag `v0.53.0`, APK). Das braucht dein Okay.
- **Grundlage (Nachtrag 14.10.):** 0.53 steht auf dem bisherigen Boulder, also auf Gravel 0.3.3. Der
  Entscheid vom 05.10. verlangt den Neubau der App auf dem Original von Core Devices
  (`entscheidungen.md`, „Grundlage“); dort ist er der letzte Schritt („App-Teile“). **F19: A**
  (14.10.): 0.53 erscheint als Brücke für den Gerätetest, der Neubau folgt als „App-Teile“. `main`
  steht auf `aecd3350`.

## 12. Kieselstrasse (14.10.2026, `18d1a41c`, `d4e15214`, `fef08c2e`)

Punkt 4 aus Abschnitt 7 (F6). Belege: `belege-vorab/v2-b28/`.

- **Port aus b26** (`main` `a7d8b16e1`, `src/fw/apps/system/kieselstrasse/` und die zwei Tests):
  - Dieselbe Kennung, dieselben Schlüssel und derselbe Persist wie die Store-App. Der Persist bleibt
    68 Byte: `time_t` ist im Firmware-Bau weiter 32 Bit (nachgeprüft mit SDK 0.1.11).
  - Eigener Schalter `CONFIG_BOULDER_KIESELSTRASSE` (default y). Er hängt an der Variante, darum gibt
    es Kieselstrasse wie Kieselsport nur auf obelix und qemu_emery. Jede `.c` steht ganz darunter.
  - Registry-ID -104, direkt neben Kieselsport im selben Hunk (P3). In V2 trägt -103 Kieselsport.
  - Kleine Anpassungen: Includes für V2, die Zeile je Nachricht als DEBUG statt INFO (Upstreams Regel
    für Logs, die oft kommen), der Kommentar zur Store-App (V2 blendet sie nicht aus, V9).
- **K1-Wächter** (`18d1a41c`): Die Ordner von Kieselstrasse sind frei. Im Registry-JSON muss jetzt
  jeder neue Eintrag einen der App-Schalter in `"ifdefs"` tragen. Vorher genügte es, dass der Hunk
  den Schalter von Kieselsport irgendwo nannte; das hätte einen zweiten Eintrag ohne Schalter gedeckt.
  Sechs neue Gegenproben.
- **Prüfbau-Ballast** (`fef08c2e`): Der Kernel-Heap von obelix im Release-Bau ist mit Kieselstrasse
  72480 B (b27: 72576 B). Der Prüfbau stand noch auf 72736 B aus Schritt 3 AP6.
- **Prüfung:**
  - Acht Bauten ohne Warnung. Kieselstrasse steckt nur in den vier mit Variante (`nm`).
  - Host 440/440, darunter `test_kieselstrasse_weg`, `test_kieselstrasse_app` und
    `test_boulder_kieselstrasse_unbenutzt`. Alle Werkzeuge grün; im Prüfbau ist
    `pruefe_ohne_rechte` wie bisher nur an der Fehlerprobe rot.
  - Mutationen F6a–F6d rot: Registry-Eintrag ohne Schalter (Wächter), gleiche Abbiegeart an anderer
    Lage übersehen, Summ-Pause 30 s, zu kurzer Persist gelesen.
  - **Anzeige gegen b26:** Beide als Demo-Bau (`-DKST_DEMO`) in QEMU, je 20 Bilder aller Abbiegearten.
    Sie sind Pixel für Pixel gleich bis auf die Minutenziffern von Uhrzeit und Ankunft (die Läufe
    lagen zwei Minuten auseinander).
  - QEMU-Messung der Health-App auf `fef08c2e`, zwei Läufe: alle Prüfungen grün. L1 höchstens 12,7
    bzw. 17,6 ms, erster Start (icount) 19,5 ms, L2 18,6 ms, L4 8,8 ms, L6 höchstens 0,02 ms;
    Kernel-Heap ohne Wachstum, frei 256 B weniger als bei b27 (der Ballast folgt jetzt 72480 B); kein
    Neustart.
  - Die acht Bauten noch einmal auf `fef08c2e`: ohne Warnung.
- **Vorabversion b28:** `normal_obelix_pvt_v4.41.0-b28.pbz` (sha256 `8bb7e9b0…a14ec`), beide Slots
  ohne Warnung, nicht signiert, an den Nutzer zum Öffnen in Boulder. Das Tag ist annotiert wie bei
  b27: Der Bau liest nur annotierte Tags (ein leichtes ergab `v4.41.0-b27-3-gfef08c2e`).
- **Befund: Store-App löschen löscht den Speicher der eingebauten App.** V2 entfernt beim Löschen
  einer Store-App die Persist-Datei ihrer Kennung. Kieselsport (Dienst) und Kieselstrasse benutzen
  sie mit; b26 sperrte das in `app_install_manager.c`. **Nutzer: nichts ändern** (nur eigene Daten,
  nichts veröffentlicht; `entscheidungen.md`).
- **Auf der Uhr (Nutzer, 14.10.):** Kieselstrasse mit b28 getestet, läuft. Damit ist auch der Weg
  Boulder → Uhr über die Kennung der Store-App bestätigt.

## 13. Upstream v4.41.1 (14.10.2026, `cf8a54f66`, `d3d17b338`, `8075766d8`)

F22 A (`entscheidungen.md`): Upstreams Patch-Fassung v4.41.1 vor dem Gerätetest, als b29. `main`
bleibt eingefroren (F18). Belege: `belege-vorab/v2-b29/`.

- **Was v4.41.1 korrigiert:** die Worker von Apps. Seit v4.41.0 war `WorkerState` 648 statt
  höchstens 640 Byte groß; `worker_state_configure()` scheiterte, kein Worker startete (in b28
  nachgemessen). Ein so gescheiterter Start blieb als laufend stehen, der nächste assertierte. Jetzt
  640 Byte, der Bau prüft die Größe, und ein gescheiterter Start wird zurückgerollt.
- **Merge** (`d3d17b338`): ohne Konflikt, keine Boulder-Stelle berührt. Kraepelin-Originale und
  Vertragsdateien unverändert, auch bis zum heutigen `main`; HERKUNFT nennt v4.41.1.
- **K1-Wächter** (`cf8a54f66`): v4.41.1 zweigt von v4.41.0 ab und liegt nicht auf `main`. Der Wächter
  nimmt jetzt das jüngste Upstream-Tag in HEAD als Basis, wenn es von der merge-base mit `main`
  abzweigt; sonst wie bisher. Drei Gegenproben an echten Git-Repos, mit der alten Regel ist die erste
  rot.
- **Prüfbau-Ballast** (`8075766d8`): Kernel-Heap von obelix im Release-Bau 72512 B (b28: 72480 B).
- **Prüfung:** acht Bauten ohne Warnung (Merge und Endstand), Host 440/440, alle Werkzeuge grün.
  QEMU zweimal grün: L1 höchstens 12,7 ms, erster Start 19,5 bzw. 20,5 ms, L2 24,4 bzw. 14,6 ms, L4
  höchstens 8,8 ms, L6 0,02 ms; Kernel-Heap ohne Wachstum, kein Neustart. L2 kam in Lauf a auf
  24,4 ms (bisher höchstens 19,5); Lauf b 14,6 ms. Das schwankt mit dem Host, die Grenze ist 30 ms.
- **Vorabversion b29:** `normal_obelix_pvt_v4.41.1-b29.pbz` (sha256 `f1e053e3…8bd31`), an den Nutzer
  für den Gerätetest ab dem 14.10. abends.
- **Nicht geprüft:** ein Worker in QEMU (keine eingebaute App hat einen); belegt ist die Ursache, die
  Größe von `WorkerState`.

## 14. Health-App: Unten blättert vorwärts (14.10.2026, `a3652f881`)

F23 (Nutzer, auf der Uhr mit b29): „In Health sind die Buttons verkehrt. Nach unten soll vorwärts
sein.“ Ersetzt F11 A. Belege: `belege-vorab/v2-b30/`.

- **Umgesetzt:** Unten blättert zur nächsten Karte, Oben zurück, nach jedem Start. Bisher blätterte
  Oben weiter wie in Pebbles Health-App, nach Quick Launch Unten gespiegelt; das Feld `gespiegelt`
  fällt weg. Kreis und erste Karte bleiben wie bei Pebble: Nach Quick Launch führt Oben auf Today zum
  Zifferblatt, Unten auf Settings tut nichts. Im Detail von Precision wählen Oben und Unten wie bisher
  die Stufe.
- **Bekannt:** Nach Quick Launch Oben fällt Pebbles Shutter beim Schliessen weiter nach unten, auch
  wenn Oben die App schliesst.
- **Tests:** `test_bh_einstieg`, `test_bh_einstiege` (gegen Pebbles Shell) und `test_bh_app` auf die
  neue Regel umgestellt; `qemu_health.py` blättert mit Unten. Mutation K5a (Oben wieder vorwärts)
  rot.
- **Prüfung:** acht Bauten ohne Warnung, Host 440/440, alle Werkzeuge grün; QEMU zweimal grün (L1
  höchstens 17,6 ms, erster Start 20,5 ms, L2 23,4 ms, L4 10,7 ms, L6 0,02 ms; Kernel-Heap ohne
  Wachstum, kein Neustart). Die QEMU-Bilder, mit Unten aufgenommen, kommen in der Reihenfolge Today,
  Night, Heart, Activity, Settings.
- **Vorabversion b30:** `normal_obelix_pvt_v4.41.1-b30.pbz` (sha256 `c64063ce…78dde4`).

## 15. Velo nur mit Puls (16.10.2026, `b3d0828de`)

Meldung des Nutzers (mit b30): Beim Herumgehen fragt die Uhr nach einer Velofahrt. Entscheid F32.
Belege: `belege-vorab/v2-b31/`.

- **Nachgestellt:** Wachminuten der echten Nächte, 8 Tage (Abend vor dem Einschlafen, Morgen nach dem
  Aufwachen). 656 von 723 bewegten Minuten waren velo-artig; an drei Tagen entstand ein
  Kandidatinnen-Fenster (03.10. 07:24, 09.10. 07:57, 12.10. 20:09). Diese Fenster hatten fast 0
  Schritte, VMC-Median 1100–4000, gleiche Lage und höchstens einen Pulswert, die Pulsprüfung entfiel
  also ganz. Puls in den velo-artigen Wachminuten: Median +23 über dem Ruhepuls (p75 +31, p90 +39).
  Die echte Fahrt vom 29.9. lag ab +66.
- **Umgesetzt:** Velo verlangt mindestens drei Pulswerte im Fenster (die Vorstufe misst ab der dritten
  velo-artigen Minute dicht) und einen Median ab 30 über dem Ruhepuls (`KONTEXT_VELO_FENSTER_PULS`,
  bisher 20). Auto bleibt.
- **Tests:** `armarbeit_zu_hause_fragt_nicht` (echte Minuten vom 12.10. abends, Puls wie gemessen +24),
  `ohne_puls_keine_velofrage`, `velo_ab_30_ueber_dem_ruhepuls` (+29 nein, +30 ja). Mutationen:
  Schwelle +20 macht den ersten und dritten rot, Fenster ohne Puls den zweiten.
- **Prüfung:** acht Bauten ohne Warnung, Host 440/440, alle Werkzeuge grün (im Prüfbau nur die
  Fehlerprobe rot). QEMU nicht wiederholt: eine Bedingung je Minute ohne Zeitbedarf.
- **Vorabversion b31:** `normal_obelix_pvt_v4.41.1-b31.pbz` (sha256 `484ee31e…010b0d`).
- **Offen:** echte Fahrten für die positive Seite. Sie kommen mit dem Minutenexport (F33).

## 16. Eingeschlafen ist, wer still liegt (17.10.2026, `688b30974`)

Nacht 16.→17.10. neben einem Fitbit: Boulder (b31) zählte 22:14 bis 07:24, Fitbit 01:38/01:46 bis
06:57. Der Nutzer bestätigt Fitbit: Bis 01:38 lag der Nutzer mit dem Handy im Bett. Entscheid F37.
Analyse: `naechte/analyse-2026-10-17.md`; Belege: `belege-vorab/v2-b32/`.

- **Befund:** Mit dem Handy regt man sich jede Minute ein wenig (VMC 100–700). Das bleibt unter
  Cole-Kripkes Schwelle, und die Wälzregel schob das Einschlafen nur eine halbe Stunde. VMC 0 hatte
  dort jede fünfte Minute, in Fitbits Schlaf 86 % der Minuten.
- **Umgesetzt:**
  - **Einschlafen:** In 30 Minuten ab dem Einschlafen sind mindestens 20 ganz still. Geprüft nach den
    Schüben, danach gilt die Wälzregel noch einmal; ohne stille halbe Stunde bleibt das Einschlafen.
  - **Aufwachen:** Ein späterer Schlaflauf verlängert die Nacht nur, wenn die Hälfte seiner ersten
    Viertelstunde still ist, und nur wo ein Lauf der Nacht still beginnt.
  - 16.10. damit 01:39 bis 06:57. Alle älteren Nächte und die Snapshot-Nacht 26.9. rechnen gleich.
- **Tests:** `test_boulder_schlaf` mit der Nacht 16.10. (Fitbits Phasen), zwei gebauten Nächten und
  derselben Nacht ohne VMC 0 (wie bis b31). Mit Absicht anders: `gang_in_der_letzten_stunde`, 30.9.
  mit Gang 35 min vor dem Aufwachen (die Nacht endet am Gang). Zwölf Mutationen rot, eine im belegten
  Bereich nicht erkannt (zehn statt fünfzehn Minuten Anfang).
- **Prüfung:** acht Bauten ohne Warnung, Host 440/440, alle Werkzeuge grün (im Prüfbau nur die
  Fehlerprobe rot). QEMU nicht wiederholt: zwei Schleifen über die Minuten der Nacht; auf dem Rechner
  ist die Analyse nicht langsamer als bei b31.
- **Vorabversion b32:** `normal_obelix_pvt_v4.41.1-b32.pbz` (sha256 `4681fd1e…58a26a`).

## 17. Ohne Velo und Auto, nachts alle drei Minuten (20.10.2026, `4c49e8270`, `0be700c93`, `9caccff25`)

Meldung des Nutzers (mit b32): „Gefühlt brauchen wir seit dem neu schreiben der Anwendung mehr Akku.
Die Fahrräder Kennung ist auch immer noch zu aggressiv.“ Entscheid F38 A, dazu die Auto-Erkennung.
Analyse: `akku-2026-10-19.md`; Belege: `belege-vorab/v2-b33/`.

- **Befund:**
  - Die Velo-Vorstufe hielt den Pulssensor ab der dritten velo-artigen Minute im Sekundentakt an. An
    den beiden V2-Abenden lief sie nach den Minuten bis zu 41 und 39 min, ohne Fahrt.
  - Nachts kam unter V2 auf „Detailed“ alle 2 Minuten ein Pulswert, unter b26 alle 3: Pebbles
    Messung zählt den Takt erst ab dem Ausschalten. Die REM-Regel ist an Nächten mit dem Takt von b26
    eingestellt.
- **Umgesetzt:**
  - Entfernt: `kontext.c`, `nachfrage.c`, `szene.c` (Kandidatin, Frage, Fahrtsitzung, Pulskalorien
    F7, Ablage „fahrt“, Szene HighIntensity), das Abo „Aktiv“ in `puls.c`, Kieselsports Meldung des
    Maximums für die Fahrt und die Prüfnacht 5. Den Merker „Training“ (Nacht, D17) führt jetzt der
    Dauerteil.
  - Kopie: D3, D4, D5 und D10 fallen weg. Gehfilter, Health-Sync, Szene und das Abo bei Aktivität
    rechnen wieder wie das Original. D12 behält den Puffer 15+19+5, D13 schreibt die Gruppen, die der
    Callback noch nicht schrieb, D15 setzt nur den Merker.
  - Stufe 4 misst nachts alle 3 statt 2 Minuten (`messstufe.c`).
- **Tests:** Die Trainingstests aus `test_boulder_fahrt` (T4.1, Pause, P4) stehen in
  `test_boulder_training`. Neu ist `ruhiges_training_ist_kein_schlaf` (Kieselsport und Pebbles
  Workout-App). `test_boulder_messung` erwartet auf Stufe 4 20 Fenster je Stunde (gemessen 21).
  Mutationen rot: F38a („ohne Schritte“ wirkt ohne Training), F38b (Start und Stopp löschen es
  nicht), F38c (ein ruhiges Training zählt für die Nacht), F38d (Detailed nachts wieder alle 2
  Minuten), F38e (D13 schreibt nicht).
- **Prüfung:** acht Bauten ohne Warnung, Host 440/440, alle Werkzeuge grün (im Prüfbau nur die
  Fehlerprobe rot). Kernel-Heap von obelix (Release) 72 736 B (+224), der Prüfbau mit gleich
  viel. QEMU zweimal grün: L1 höchstens 14,6 ms, erster Start 20,5 ms, L2 13,7 ms, L4 8,8 ms, L6
  0,02 ms; Kernel-Heap ohne Wachstum, kein Neustart.
- **Vorabversion b33:** `normal_obelix_pvt_v4.41.1-b33.pbz` (sha256 `dfd497ed…709da8`).
- **Offen:** der Akku selbst. Boulders Seite „Battery“ zeigt den Verbrauch je Tag und je Nacht; den
  Vergleich mit b32 bringen die nächsten Tage.

## 18. Upstream v4.41.2 und main (20.10.2026, `97ae1df54`, `d5cc0038c`)

F39 („Alles aus dem upstream übernehmen“, `entscheidungen.md`): Damit endet das Einfrieren bis nach
dem Gerätetest (F18). Belege: `belege-vorab/v2-b34/`.

- **v4.41.2** (`97ae1df54`), ohne Konflikt:
  - Auf obelix sind Pebbles Menüzeilen in der Standardgröße wieder 50 statt 61 px hoch.
  - Drei Benachrichtigungs-Änderungen aus v4.41.0 sind zurückgenommen. Eine weggewischte
    Benachrichtigung, die das Telefon noch einmal schickt, wird wieder gespeichert. Bearbeitete
    stehen wieder im Verlauf und in der Vorschau des Launchers.
- **`main`** (`d5cc0038c`, 395 Commits nach v4.41.0, bis `723a1708c`):
  - Konflikte gab es nur in Pebble-Dateien ohne Boulder-Stelle; dort gilt `main`.
  - Submodule: moddable und pebbleos-nonfree sind nachgezogen, fff ist neu.
  - Boulder ist an Upstreams Umbauten angepasst:
    - Syscalls mit `PBL_SYSCALL` und Argumentarten.
    - `struct pbl_tm` statt der überschatteten `time.h`.
    - `pbl/kernel/assert.h` statt `passert.h`.
    - Lichtsensor über `light_level_to_enum`.
    - Syscall-Stapel in `pebble_tasks.c`.
    - Test-Stubs als Bibliothek, Testmodule.
  - Kopie: Upstreams Änderungen an den Originalen kamen per `git merge-file`, darunter das neue
    `activity_algorithm_get_minute_heart_rate`. Es liest über dieselben Funktionen wie der
    Minutenverlauf, also mit D7 und D12. `HERKUNFT` nennt `723a1708c`.
  - **Fund beim Anpassen:** `wecklauf.c` hätte nach dem Umbau den Wochentag still mit `localtime_r`
    der C-Bibliothek (UTC) gerechnet, ohne Fehler beim Bauen. Jetzt `pbl_localtime_r`.
  - **K2:** Den Rumpf von S2 legt der Compiler jetzt in eine eigene Funktion
    (`sys_boulder_lage__impl.isra.0`). `mehrwort.sh` prüft solche Rümpfe mit; sonst bliebe G-K2
    grün.
  - S3 prüft wie bisher nur, was kopiert wird, und ohne Wunsch nichts, jetzt mit
    `pbl_syscall_verify_user` im Rumpf. Upstream spiegelt Stapel-Argumente auf den Syscall-Stapel,
    die Zusicherung für vier Register-Argumente entfällt.
  - Kernel-Heap von obelix (Release) 70 432 B (−2 304), der Prüfbau mit gleich viel.
- **Prüfung:** acht Bauten ohne Warnung, Host 466/466 (26 neue Tests von Upstream), alle Werkzeuge
  grün (im Prüfbau nur die Fehlerprobe rot). Mutationen 54/54 rot. QEMU zweimal grün:
  L1 höchstens 13,7 ms, erster Start 23,4 ms (b33: 20,5), L2 14,6 ms, L4 6,8 ms, L6 0,01 ms;
  Kernel-Heap ohne Wachstum, Syscall-Stapel mit 1 368 B Reserve, kein Neustart. Die Health-App
  liest über die umgestellten S1–S7 wie bisher.
- **Vorabversion b34:** `normal_obelix_pvt_v4.41.2-b34.pbz` (sha256 `956b987b…21fb83`).

## 19. Karten gleiten wie bei Pebble (21.10.2026, `967da424b`, `685b50fe9`, `e044bda27`, `a9133d56e`)

F40 („Können wir in Health vertikal scrollen?“ – „A. Wir bleiben mit allem was wir machen so nahe an
Pebble wie es geht“, `entscheidungen.md`). Belege: `belege-vorab/v2-b35/`.

- **Gleiten** (`685b50fe9`) wie in Pebbles `card_view.c`:
  - Je Karte eine Ebene; die gezeigte gleitet hinaus, die neue herein.
  - Kurve und Dauer sind Pebbles: moook mit einem Bild in der Mitte, 264 ms.
  - Die Richtung folgt der Taste: Oben von oben, Unten von unten, auch beim Wischen.
  - Eine Taste mitten im Gleiten bricht es ab wie bei Pebble.
  - Beim Federn am Ende (bis 4 px) zeigt das Fenster die Farbe, die an die neue Karte grenzt.
- **Details** öffnen wieder mit Pebbles Übergang. AP8 hatte ihn für L2 weggelassen.
- **Ohne Systemrechte:** Pebbles Kurve steht nicht im SDK. `pruefe_ohne_rechte.py` führt drei Helfer
  mehr; Pebbles Applib nutzt sie selbst in der App-Task. Die Probe der Bausteine benutzt sie in QEMU:
  44 Bausteine, „vollständig“, kein Fehler.
- **L2** (`a9133d56e`) begrenzt den Anteil der App mit 30 ms: Taste bis zum ersten Bild des Gleitens
  und Zeichnen des Details. Wann das Neue zu sehen ist, bestimmt Pebble: die neue Karte ein Bild nach
  dem Beginn, das Detail mit dem ersten Bild von Pebbles Übergang (in QEMU nach 40–60 ms). Ein Gleiten
  und ein Detail zum Aufwärmen zählen nicht (QEMU übersetzt Pebbles Animationscode beim ersten Mal,
  wie F17).
- **Nebenbefunde:**
  - `967da424b`: QEMU zählt 1000 Ticks je Sekunde, `qemu_health.py` rechnete mit 1024. Alle
    bisherigen QEMU-Zeiten waren 2,4 % zu klein; kein Urteil kippt.
  - `e044bda27`: Die Probe der Bausteine endete seit b34 beim Schliessen an der MPU. Seit Upstream
    `main` schreibt `menu_layer_destroy` für eine App mit System-ID Kernel-RAM. Die Health-App benutzt
    keine MenuLayer, Kieselsport läuft mit Systemrechten; die Probe kommt jetzt ohne aus, und
    `pruefe_ohne_rechte.py` meldet `menu_layer_destroy`.
- **Prüfung** auf `e044bda27` (`a9133d56e` ändert nur `qemu_health.py`): acht Bauten ohne Warnung,
  Host 466/466, alle Werkzeuge grün (im Prüfbau nur die Fehlerprobe rot). QEMU zweimal grün auf
  `a9133d56e`:
  - L1 höchstens 16 ms, erster Start (icount) 26,0 ms.
  - L2 (Anteil der App) höchstens 18 ms. Die Karte ruht nach 265–278 ms, die neue ist nach 35–70 ms
    zu sehen, das Detail nach 52,5 bzw. 55,5 ms im Median.
  - L4 höchstens 7 ms, L6 0,02 ms.
  - Kernel-Heap ohne Wachstum (51 408 → 51 332 → 51 332 B), App-Heap höchstens 5 748 B,
    Syscall-Stapel mit 1 368 B Reserve, kein Neustart.
