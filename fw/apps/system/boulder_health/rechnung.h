/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// Die Rechnungen der Health-App: Urteile, HRV-Band, Pulszonen und Formate (Spezifikation
// Schritt 3, AP5). GERECHNET WIE BOULDER (Boulder 34fc13cb: Tagesurteile.kt,
// Gesundheitsrechnung.kt, Formate.kt): Uhr und Telefon sagen am selben Morgen dasselbe.
// test_bh_rechnung prueft das gegen ein Orakel aus Boulders eigenem Kotlin.
//
// REIN: nur Argumente hinein, Ergebnis heraus. Kein Syscall, kein Zustand, keine Gleitkommazahl.
// So darf das Zeichnen sie beliebig oft rufen, ohne die Ladeziele zu beruehren (L2, L3), und
// das Ergebnis haengt nicht an der Rundung einer Gleitkomma-Bibliothek.

#include "applib/graphics/gtypes.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

// --- Tage ---

// Ein Tag als Zahl der Tage seit dem 1.1.2000 (Ortszeit), wie die Variante ihre Naechte und
// ihre Woche fuehrt. Aus tm_year und tm_yday von sys_localtime_r.
uint16_t bh_tag(int tm_year, int tm_yday);
// 0 = Montag .. 6 = Sonntag
uint8_t bh_wochentag(uint16_t tag);

// --- Urteile ---

typedef enum {
  BhSchlafAusgeruht, //!< "Well rested": mindestens das Ziel
  BhSchlafKurz,      //!< "Short night": bis eine Stunde darunter
  BhSchlafSehrKurz,  //!< "Very short night"
} BhSchlafurteil;

// Tagesurteile.kt:86-90 mit dem Schlafziel aus der Lage.
BhSchlafurteil bh_schlafurteil(uint16_t minuten, uint16_t ziel_min);

typedef enum {
  BhVergleichKeiner,   //!< zu wenig fruehere Werte oder kein Wert: kein Urteil
  BhVergleichWieImmer, //!< "as usual" (Ruhepuls), "in range" (HRV)
  BhVergleichHoeher,   //!< "+3" (Ruhepuls), "above" (HRV)
  BhVergleichTiefer,   //!< "-3" mit Minus U+2212 (Ruhepuls), "below" (HRV)
} BhVergleich;

// Ein Wachtag der Naechte (S5, Spezifikation 3.3): der Ruhepuls der laengsten Nacht des Tags,
// die HRV der laengsten Nacht mit HRV. 0 = kein Wert. Die Rechnungen nehmen die Tage in jeder
// Reihenfolge (die Variante fuehrt einen Ring), aber jeden Tag nur einmal.
typedef struct {
  uint16_t tag;
  uint16_t hrv;
  uint8_t ruhepuls;
} BhNachtTag;

typedef enum {
  BhWertRuhepuls,
  BhWertHrv,
} BhWert;

// Der Schnitt der frueheren Naechte vor dem Wachtag `tag`, wie Boulder ihn waehlt
// (Tagesurteile.kt:138-146, :209-228): hoechstens die sieben juengsten Wachtage der acht Tage
// davor, dann ohne Werte 0. Liefert die Zahl der Naechte mit Wert und in *schnitt das Mittel,
// gerundet wie Math.round (0 ohne Nacht). Ab drei Naechten steht es als "avg" da.
uint8_t bh_naechte_schnitt(const BhNachtTag *tage, uint8_t anzahl, uint16_t tag, BhWert wert,
                           uint16_t *schnitt);

// Der Ruhepuls der Nacht von heute (`letzte`, 0 = keiner) gegen die frueheren Naechte
// (Tagesurteile.kt:119-136): ab drei frueheren Naechten, "as usual" bis zwei Schlaege neben dem
// gerundeten Schnitt. *abstand in bpm, 0 ohne Urteil.
BhVergleich bh_ruhepuls_urteil(uint8_t letzte, const BhNachtTag *tage, uint8_t anzahl,
                               uint16_t heute, uint8_t *abstand);

#define BH_WOCHE_TAGE 7

// Ein Tag der HRV-Woche mit seinem Band aus den 30 Tagen davor (Gesundheitsrechnung.kt:488-522).
typedef struct {
  uint16_t tag;
  uint16_t wert;  //!< HRV des Wachtags, 0 = keine
  uint16_t unten; //!< Band, nur wenn `band`; unten ist mindestens 1
  uint16_t oben;
  uint8_t frueher; //!< Werte in den 30 Tagen davor
  bool band;       //!< ab drei frueheren Werten
} BhBandtag;

// Die sieben Tage bis heute, aeltester zuerst, heute in woche[6] (wie Boulders hrvWoche).
void bh_hrv_woche(const BhNachtTag *tage, uint8_t anzahl, uint16_t heute,
                  BhBandtag woche[BH_WOCHE_TAGE]);
// Ob das Band urteilt: erst ab sieben frueheren Werten. Davor steht es nur im Bild.
bool bh_band_urteilt(const BhBandtag *tag);
// Der Wert des Tags an seinem Band (Tagesurteile.kt:148-171). *abstand bis zum Bandrand.
BhVergleich bh_band_urteil(const BhBandtag *tag, uint16_t *abstand);

// Ein Puls ist "jetzt", solange er hoechstens eine Viertelstunde alt ist (Tagesurteile.kt:71,
// :192).
bool bh_puls_frisch(time_t gemessen, time_t jetzt);

// Schritte am Ziel: die volle Saeule (Schrittwoche.kt:72).
bool bh_ziel_erreicht(uint32_t schritte, uint32_t ziel);
// Wie weit ein Balken der Laenge `laenge` bis zum Ziel gefuellt ist, abgerundet; am Ziel und
// darueber ganz, ohne Ziel leer (wie Boulders Anteil, Widgetlage.kt:236-239).
uint16_t bh_balken(uint32_t wert, uint32_t ziel, uint16_t laenge);

// --- Pulszonen ---

#define BH_ZONEN 4

// Karvonen 40/60/80 % der Pulsreserve (Gesundheitsrechnung.kt:475-478), die Reserve mindestens
// 20. Ohne Ruhepuls-Median (0) gilt 60 wie in Boulder (Gesundheitsdaten.kt:531).
void bh_puls_grenzen(uint8_t ruhepuls_median, uint16_t maxpuls, uint16_t grenzen[BH_ZONEN - 1]);
// 0 Light, 1 Moderate, 2 Vigorous, 3 Peak (Gesundheitsrechnung.kt:481)
uint8_t bh_puls_zone(uint16_t bpm, const uint16_t grenzen[BH_ZONEN - 1]);
GColor bh_zonenfarbe(uint8_t zone);

// --- Formate (Formate.kt) ---

// LECO hat weder den Strich U+2013 noch das Minus U+2212 (Spezifikation 2.5): in LECO steht "-".
typedef enum {
  BhSchriftGothic,
  BhSchriftLeco,
} BhSchrift;

// Zahl und Einheit getrennt, damit die Einheit kleiner daneben stehen kann (Formate.Angabe).
typedef struct {
  char zahl[16];
  const char *einheit; //!< NULL ohne Einheit
} BhAngabe;

// Der Strich fuer einen fehlenden Wert: U+2013 in GOTHIC, "-" in LECO.
const char *bh_fehlt(BhSchrift schrift);
// Eine Dauer: ab einer Stunde "7:12" mit "h", darunter "45" mit "min".
void bh_dauer(uint16_t minuten, BhAngabe *aus);
// Eine Dauer als "7:12", ohne Einheit.
void bh_stunden_minuten(uint16_t minuten, char *aus, size_t platz);
// Eine ganze Zahl mit Tausendertrennern: "6,230", negativ mit Minus (U+2212, in LECO "-").
void bh_zahl(int32_t zahl, BhSchrift schrift, char *aus, size_t platz);
// Ein Unterschied mit Vorzeichen: "+3", Minus U+2212 und "3", U+00B1 und "0". Nur fuer GOTHIC.
void bh_vorzeichen(int32_t zahl, char *aus, size_t platz);
// Eine Strecke mit einer Stelle: "4.8" mit "km" oder "mi".
void bh_strecke(uint32_t meter, bool meilen, BhAngabe *aus);
// Eine Uhrzeit aus der Minute des Tages: "06:00" oder "6:00 AM".
void bh_uhrzeit(uint16_t minute, bool h24, char *aus, size_t platz);
// Prozent ohne Abstand: "96%".
void bh_prozent(uint8_t prozent, char *aus, size_t platz);
// Der Wochentag in zwei Buchstaben: "Mo" .. "Su".
const char *bh_wochentag_kurz(uint8_t wochentag);
