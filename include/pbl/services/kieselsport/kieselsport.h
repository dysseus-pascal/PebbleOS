/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// Kieselsport: das Training als Dienst der Firmware.
//
// WARUM IN DER FIRMWARE UND NICHT MEHR IM WORKER: der Worker aus dem Store
// durfte nicht brummen, nicht mit dem Telefon reden und musste fuer jeden
// Zonenwechsel die App nach vorn holen. Hier laeuft das Training auf
// KernelBG, brummt selbst und kennt dieselben Sensoren wie die eingebaute
// Workout-App. Die App ist nur noch Anzeige und Tasten.
//
// DER SPEICHER IST DER DER APP: der Dienst schreibt unter der Kennung von
// Kieselsport (KIESELSPORT_UUID). So sehen die System-App und der Dienst
// dasselbe Archiv, dieselben Einstellungen und dieselbe wartende
// Zusammenfassung - wie vorher App und Worker.

#include "pbl/util/uuid.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// 6c386250-a6d7-4a19-a485-52541b5d7c0d, dieselbe wie die Store-App.
#define KIESELSPORT_UUID_INIT                                                             \
  {0x6c, 0x38, 0x62, 0x50, 0xa6, 0xd7, 0x4a, 0x19, 0xa4, 0x85, 0x52, 0x54, 0x1b, 0x5d, \
   0x7c, 0x0d}
#define KIESELSPORT_UUID ((Uuid)KIESELSPORT_UUID_INIT)

#define KS_ZONEN 5

// NEUE ARTEN KOMMEN HINTEN DAZU, niemals dazwischen. Die Zahl geht ans
// Telefon und steht dort in der Gesundheitsakte; eine eingeschobene Zeile
// machte aus jedem gespeicherten Wandern rueckwirkend ein Krafttraining.
typedef enum {
  ArtLaufen = 0,
  ArtBike,      //< Strasse UND Gravel - dasselbe
  ArtWandern,
  ArtKraft,
  ArtMTB,
  ArtYoga,
  ArtSchwimmen,
  ArtAnzahl,
} Sportart;

// Was eine Art an Kennzahlen hergibt. Zeit und Puls koennen alle.
typedef struct {
  bool schritte;       //< Schritte zaehlen sinnvoll?
  bool distanz;        //< Strecke aus Schritten ableitbar?
  bool reps;           //< Wiederholungen und Pausen zaehlen?
  bool bahnen;         //< Bahnen ueber den Kompass zaehlen?
} ArtInfo;

typedef enum {
  LaufAus = 0,
  LaufLaeuft,
  LaufPause,
} Laufzustand;

// DIESELBE ANORDNUNG WIE IN KIESELSPORT. Das Archiv liegt als rohe Bytes im
// Persist; wer hier ein Feld verschiebt, liest die Trainings der Store-App
// als Unsinn.
typedef struct {
  uint8_t art;
  uint32_t beginn;        //< Unix-Sekunden
  uint32_t dauer_s;       //< ohne Pausen
  uint32_t schritte;
  uint32_t meter;
  uint32_t kcal;
  uint16_t puls_mittel;
  uint16_t puls_max;
  // NUR BEI DEN ARTEN, DIE SIE HERGEBEN. Null heisst hier: gibt es nicht.
  uint16_t saetze;      //< Kraft
  uint16_t reps;        //< Kraft, alle Wiederholungen zusammen
  uint16_t bahnen;      //< Schwimmen
  uint16_t hrv_ms;      //< Yoga: RMSSD, 0 wenn nicht gemessen
} Trainingsstand;

// Wie das letzte Training endete - fuer die App, die auf Speichern wartet.
typedef enum {
  KsEndeKeins = 0,
  KsEndeGespeichert,   //< liegt als Zusammenfassung im Persist
  KsEndeVerworfen,     //< unter einer Minute oder verworfen
} KsEnde;

// DIE LETZTE MINUTE fuer die Pulskurve der App, Platz = Sekunde % 60.
// 0 heisst: kein frischer Puls, die Kurve hat dort eine Luecke.
#define KS_MINUTE_S 60

// Alles, was die App jede Sekunde zeigt - ein Schnappschuss, unter der
// Sperre des Dienstes kopiert.
typedef struct {
  Laufzustand zustand;
  Sportart art;
  Trainingsstand t;
  uint16_t puls;           //< 0, wenn keiner zu haben ist
  bool frisch;             //< oder nur der letzte gute Wert von vorhin
  uint8_t zone;            //< 0 = unter Zone 1, nur bei frischem Puls
  uint16_t maxpuls;
  uint16_t reps_laufend;   //< Kraft: Wiederholungen im laufenden Satz
  bool ruht;               //< Kraft: Pause zwischen zwei Saetzen?
  uint16_t ruhe_s;         //< Kraft: wie lange die Pause schon dauert
  bool bahnen_bereit;      //< Schwimmen: weiss der Kompass, wo er ist?
  bool sparsam;            //< schwacher Akku, Puls nur alle fuenf Sekunden
  uint16_t hrv_anzahl;     //< Yoga: Intervalle bisher
  uint8_t minute[KS_MINUTE_S];
  KsEnde ende;             //< wie das letzte Training endete
  uint8_t ende_nr;         //< zaehlt bei jedem Ende weiter
} KsStand;

void kieselsport_init(void);

// --- Befehle. Sie laufen auf KernelBG; der Schnappschuss zeigt die Wirkung.

// Mit vorgegebenem Beginn (0 = jetzt) - dem, den die App schon ans Telefon
// gemeldet hat. Laeuft schon ein Training, geschieht nichts.
void kieselsport_starte(Sportart art, uint32_t beginn);
void kieselsport_pause_umschalten(void);
// Beenden und fuers Telefon behalten; unter einer Minute verworfen.
void kieselsport_speichern(void);
// Beenden und vergessen - ein Fehlgriff, ein Abbruch.
void kieselsport_verwerfen(void);

void kieselsport_stand(KsStand *aus);
bool kieselsport_laeuft(void);   //< laeuft oder pausiert

// --- Arten und Zonen

const ArtInfo *kieselsport_art_info(Sportart art);
int kieselsport_zone(uint16_t bpm);
uint16_t kieselsport_zonengrenze(int zone);

// --- Einstellungen, im Persist von Kieselsport

// DER MAXIMALPULS KENNT "NICHT GESETZT". Frueher stand dafuer die Zahl 190,
// und wer 190 wirklich gemessen hatte, konnte sie nicht einstellen. Ohne
// eigenen Wert rechnet die Uhr nach Tanaka (208 - 0,7 x Alter) mit dem Alter
// aus ihren Gesundheitseinstellungen, die das Telefon ohnehin abgleicht. So
// folgen die Zonen dem Alter, ohne dass jemand etwas schicken muss.
uint16_t kieselsport_maxpuls(void);   //< der Wert, mit dem gerechnet wird
bool kieselsport_maxpuls_gesetzt(void);
void kieselsport_maxpuls_setze(uint16_t schlaege);
void kieselsport_maxpuls_nicht_setzen(void);
// EINE ZAHL OHNE ANGABE, OB SIE GESETZT IST - von einem Telefon, das das
// noch nicht kennt. Gilt wie bisher als eingestellt; nur das Echo des
// Werts, den die Uhr ohne Einstellung gemeldet hat, aendert nichts.
void kieselsport_maxpuls_alt(uint16_t schlaege);
uint8_t kieselsport_empfindlichkeit(void);        //< 1 traege, 2 normal, 3 fein
void kieselsport_empfindlichkeit_setze(uint8_t stufe);
uint16_t kieselsport_pausenziel(void);            //< Sekunden, 0 = nie brummen
void kieselsport_pausenziel_setze(uint16_t sekunden);
uint16_t kieselsport_becken(void);                //< Meter
void kieselsport_becken_setze(uint16_t meter);

// --- Archiv, index 0 ist das juengste

#define KS_ARCHIV_MAX 20
int kieselsport_archiv_anzahl(void);
bool kieselsport_archiv_lesen(int index, Trainingsstand *aus);

// --- Was auf das Telefon wartet

// Die Abschnittsliste: "beginn:anzahl:dauer;..." bis 300 Zeichen.
#define KS_LISTE_MAX 300
bool kieselsport_wartend_laden(Trainingsstand *t, char *liste, size_t platz);
// Nach der Bestaetigung des Telefons - nur, wenn die wartende noch zu
// [beginn] gehoert. Ein spaetes ACK eines frueheren Trainings loescht kein
// neueres (0 vergisst jede).
void kieselsport_wartend_vergessen(uint32_t beginn);

// Die Pausen der wartenden Zusammenfassung, als Byte fuer das Telefon:
// [0] Fassung (KS_PAUSEN_FASSUNG), [1] Zahl aller Pausen (bis 255), dann je
// Pause aktive Sekunde und Dauer in Sekunden, beide uint16 little-endian.
// Steht die Zahl hoeher, als die Liste lang ist, fehlen Pausen. Nur
// abgeschlossene Pausen; die letzte, aus der gespeichert wurde, beginnt bei
// kieselsport_wartend_ende(). Rueckgabe 0: eine Zusammenfassung aelterer
// Firmware, ohne Pausen.
#define KS_PAUSEN_MAX 32
#define KS_PAUSEN_FASSUNG 1
#define KS_PAUSEN_BYTES (2 + 4 * KS_PAUSEN_MAX)
size_t kieselsport_wartend_pausen(uint8_t *aus, size_t platz);
// Wann die aktive Zeit endete (Unix-Sekunden): der Beginn der letzten Pause.
// 0 bei einer Zusammenfassung aelterer Firmware.
uint32_t kieselsport_wartend_ende(void);

// DER STOPP EINES VERWORFENEN TRAININGS. Das Telefon zeichnet die Strecke
// auf, bis es davon hoert; ging die Meldung verloren, lief das GPS weiter.
// Also liegt er im Persist wie die Zusammenfassung, bis das Telefon ihn
// bestaetigt (vergessen nur, wenn [beginn] noch derselbe ist). Ein neues
// Training ersetzt ihn - dessen Start sagt dem Telefon ohnehin alles.
bool kieselsport_stopp_laden(uint32_t *beginn, uint32_t *zeit, uint8_t *art);
void kieselsport_stopp_vergessen(uint32_t beginn);

// Die Pulskurve: ein Wert je zehn Sekunden, in Stuecken ans Telefon.
#define KS_KURVE_TAKT_S 10
bool kieselsport_kurve_wartet(void);
uint32_t kieselsport_kurve_beginn(void);
uint16_t kieselsport_kurve_anzahl(void);
uint16_t kieselsport_kurve_ab(void);
uint16_t kieselsport_kurve_stueck(uint8_t *aus, uint16_t max);
// Das Telefon hat [n] Werte ab [ab] der Kurve von [beginn] - die Werte des
// Stuecks, das hinausging. false: inzwischen liegt eine andere Kurve oder
// ein anderer Zeiger da, die Bestaetigung zaehlt nicht.
bool kieselsport_kurve_bestaetigt(uint32_t beginn, uint16_t ab, uint16_t n);
// Fehlt der Schluessel des naechsten Stuecks? Dann ist die Kurve kaputt -
// anders als bei einem Lesefehler, der vorbeigehen kann.
bool kieselsport_kurve_luecke(void);
void kieselsport_kurve_vergessen(void);
// Die Zeit je Zone aus der gemerkten Kurve, fuer den Schirm nach dem
// Speichern. sekunden[0] ist unter Zone 1.
bool kieselsport_kurve_zeiten(uint32_t sekunden[KS_ZONEN + 1]);
