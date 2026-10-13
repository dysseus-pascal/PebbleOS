# Boulder V2 – Schritt 3: Abnahme (AP9)

Stand 12.10.2026, Zweig `v2` bei `ecb6b739`, Notizen im Zweig `notizen`.
Grundlage: Spezifikation `schritt3-rest-spezifikation.md`, Abschnitte 5 (AP9) und 7 (Kriterien, F15).
Belege: `belege-schritt3/ap9/`.

## 0. Kurzfassung

- **Kriterien:** 1, 2, 3, 4, 5 und 7 sind erfüllt (Host, QEMU, Abbild). 6 ist teilweise erfüllt: in QEMU
  ja, auf der Uhr steht der Gerätetest aus. 8 ist teilweise erfüllt: auf der Uhr ja, in Boulder 0.53 fehlt
  die Seite „Goals“.
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
| 6 | Ladezeit L1–L6 in QEMU; auf der Uhr im Gerätetest | **teilweise**: QEMU erfüllt, Uhr offen | Drei QEMU-Schlussläufe grün (Abschnitt 3). Der erste Start nach dem Boot liegt nahe an der Grenze und schwankt in Echtzeit-QEMU mit dem Host; unter `-icount` 17,6 ms wie mit dem Code von AP8. QEMU-Mutation (60 ms vor dem ersten Bild) an L1 rot (89,8 ms); Mutation K6b (Taste liest, L3) rot |
| 7 | Kein Kernel-Heap-Verlust nach 100 Starts, kein Puls-Abo nach dem Ende, kein Neustart | **erfüllt** (QEMU) | QEMU-Schlussläufe a–c; QEMU-Mutation (16 B Kernel-Heap je S2) am Kernel-Heap rot (49 372 → 48 304 → 47 304 B); Mutation K7a rot; seit A2 läuft Puls live auch nach einem abnormalen Ende höchstens 2 min |
| 8 | `boulder_ziele` vom Telefon gilt auf Uhr und in Boulder; ohne Telefon der letzte Wert oder die Vorgabe | **teilweise**: Uhr erfüllt, Boulder offen | `test_boulder_schnittstelle` (Einfügen, Ändern, Löschen, Leeren, ungültige Sätze, Neustart); Mutationen K8a, K8b rot; die Seite „Goals“ in Boulder 0.53 fehlt (Schlüssel ohne Nullbyte, K4) |

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
2. Wechsel b26 → V2 testen (bisher mit einem b25-Abbild).
3. Boulder 0.53: Seite „Goals“ schreibt `boulder_ziele` (8 B, **Schlüssel ohne Nullbyte**, Befund K4),
   V2 als Firmware erkennen, Gravel 0.3.4.
4. Kieselstrasse nach V2 portieren (F6).
5. Vorher-Video von b26 (braucht die Uhr).
6. Gerätetest: 7 Nächte, Fitbit-Nächte, Velo- und Autofahrten, Messstufen, Ladezeiten auf der Uhr
   (Kriterium 6), dazu die Grenzen oben.
7. Ruff: 45 ältere Befunde in den Boulder-Werkzeugen (`tools/boulder/`, zwei Testhelfer). Upstreams
   Python ist sauber, und die CI von PebbleOS prüft Ruff bei Pull Requests.

### Zu entscheiden

**F17 Erster Start in QEMU.** In Echtzeit-QEMU zählt beim ersten Start nach dem Boot QEMUs Übersetzen
mit. Dieser Anteil schwankt mit dem Host, darum kam der Start in AP9 auf 30–54 ms. Die übrigen 59 Starts
lagen in den Schlussläufen bei höchstens 27,3 ms.
- A: so lassen (Höchstwert über alle Starts in Echtzeit). Ein Lauf kann dann rot werden, ohne dass sich
  etwas geändert hat.
- B: `qemu_health.py --icount`: Ein zweiter Boot mit `-icount shift=3` misst den ersten Start nach dem
  Boot. Dort zählt nur, was die App ausführt (heute 17,6 ms). In Echtzeit zählen alle übrigen Starts wie
  bisher.
- **Empfehlung: B.** Es misst die Bauart (2.6.3) statt des Emulators, und das Ergebnis ist wiederholbar.
