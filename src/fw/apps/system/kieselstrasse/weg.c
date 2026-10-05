/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pebble_compat.h"
#include "schluessel.h"
#include "texte.h"
#include "weg.h"

#include "applib/graphics/utf8.h"

// Ab wann ein Stand keine Navigation mehr ist. Zehn Minuten sind lang genug
// fuer einen Stau und kurz genug, dass niemand einem Pfeil von gestern folgt.
#define KST_VERALTET_S 600

// Unter dieser Entfernung laeuft der Balken - die Spanne, in der ein Blick
// aufs Handgelenk noch etwas aendert.
#define KST_BALKEN_AB_M 300

// Weiter als zehntausend Kilometer ist keine Entfernung, sondern Muell - und
// mit Muell nahe INT32_MAX liefe die Rundung im Schirm ueber (nav_window.c).
#define KST_WEITESTE_M 10000000

// DERSELBE ABBIEGETYP NOCH EINMAL - "rechts, dann wieder rechts". Die Art
// sagt dann nichts. VERLAESSLICH IST DIE LAGE DER ABZWEIGUNG AUF DER ROUTE:
// ihre Meter bis zum Ziel, REST minus ENTFERNUNG. Faehrt man auf dieselbe
// Abzweigung zu, schrumpfen beide gleich, und die Lage bleibt bis auf ein paar
// Meter Rundung. Die naechste Anweisung liegt naeher am Ziel - die Lage
// springt um den Abstand der beiden Abzweigungen, wie schnell man auch faehrt
// und wie selten Boulder bei gleicher Art schickt (hoechstens alle vier
// Sekunden). Mehr als KST_LAGE_M ist eine neue Anweisung, auch nach einer
// neuen Route. Abzweigungen, die naeher beieinander liegen, sind eine.
#define KST_LAGE_M 30
// OHNE REST (am Ziel, ein anderer Sender) bleibt nur die Entfernung: war man
// an der Abzweigung (hoechstens KST_NAH_M davor) und springt sie danach um
// mindestens KST_SPRUNG_M nach oben, ist das die naechste. Ein Sprung um
// KST_WEIT_M gilt immer - so weit springt kein GPS. Das erkennt nur, wer
// zwischen zwei Meldungen wenig Weg macht. Ein flatternder Name zaehlt nie,
// eine Entfernung, die um ein paar Meter zappelt, auch nicht.
#define KST_NAH_M 50
#define KST_SPRUNG_M 50
#define KST_WEIT_M 500

static Weg s_weg;

// Ausfahrt und Winkel stehen neben s_weg, nicht darin (siehe weg.h).
static int s_ausfahrt;
static int s_winkel;
// Die Lage der Abzweigung aus der letzten Nachricht (siehe KST_LAGE_M), -1 =
// unbekannt. Nur im Speicher: nach dem Oeffnen gilt bis zur zweiten
// Nachricht die Regel ueber die Entfernung.
static int32_t s_lage_m;

// Weicht der Stand vom Persist ab? Nur dann lohnt ein Schreibvorgang - ein
// Blick auf die Uhr ohne eine einzige Nachricht soll den Flash nicht beruehren.
static bool s_geaendert;

static void prv_anfang(void) {
  memset(&s_weg, 0, sizeof(s_weg));
  s_weg.entfernung_m = -1;
  s_weg.rest_m = -1;
  s_ausfahrt = 0;
  s_winkel = WEG_KEIN_WINKEL;
  s_lage_m = -1;
}

// Ungueltiges UTF-8 vom Ende her abschneiden, bis der Rest gueltig ist.
static void prv_utf8_reparieren(char *text) {
  for (size_t n = strlen(text); n > 0 && !utf8_is_valid_string(text); n--) {
    text[n - 1] = '\0';
  }
}

// Eine Entfernung, die keine sein kann, ist "keine Angabe".
static int32_t prv_meter(int32_t meter) {
  return (meter >= 0 && meter <= KST_WEITESTE_M) ? meter : -1;
}

void weg_init(void) {
  prv_anfang();
  s_geaendert = false;
  if (!persist_exists(PERSIST_WEG)) return;
  // NUR EIN VOLLER STAND ZAEHLT. Ein kuerzerer oder kaputter Persist (etwa
  // von einer aelteren Fassung) liesse halbe Felder stehen - dann lieber
  // gar nichts wissen.
  if (persist_read_data(PERSIST_WEG, &s_weg, sizeof(s_weg)) != (int)sizeof(s_weg)) {
    prv_anfang();
    return;
  }
  // Der Schirm zeichnet die Strasse als C-Zeichenkette; auf den Abschluss
  // aus dem Flash verlassen wir uns nicht.
  s_weg.strasse[KST_STRASSE_LEN - 1] = '\0';
  // EIN NAME AUS EINER AELTEREN FASSUNG kann mitten in einem Zeichen enden
  // (strncpy kuerzte nach Bytes). Ungueltiges UTF-8 zeichnet die Uhr gar
  // nicht - die Zeile bliebe leer, bis die naechste Nachricht kommt. Also
  // bis zum letzten ganzen Zeichen kuerzen.
  prv_utf8_reparieren(s_weg.strasse);
  s_weg.entfernung_m = prv_meter(s_weg.entfernung_m);
  s_weg.rest_m = prv_meter(s_weg.rest_m);
  // Eine Abbiegeart von gestern darf das Summen des ersten echten Schritts
  // nicht verschlucken, nur weil er zufaellig dieselbe Art hat ("neu" setzt
  // eine Aenderung voraus). Angezeigt wird sie bei veraltetem Stand ohnehin
  // nicht. Nur im Speicher - der Persist bleibt, bis etwas Neues kommt.
  if (weg_veraltet()) s_weg.abbiegeart = 0;
}

/**
 * Eine Zeichenkette uebernehmen.
 *
 * Fehlt der Schluessel, bleibt das alte Feld stehen - so kann das Telefon die
 * Entfernung nachschieben, ohne den Strassennamen mitzuschicken.
 *
 * STEHT DER SCHLUESSEL ABER DA UND IST LEER, WIRD GELOESCHT. Sonst bliebe nach
 * dem Ende einer Navigation die letzte Strasse unter "Keine Navigation" stehen.
 */
static void prv_nimm_text(DictionaryIterator *iter, uint32_t key, char *ziel, size_t len) {
  Tuple *t = dict_find(iter, key);
  if (!t || t->type != TUPLE_CSTRING) return;
  if (t->length <= 1) {
    ziel[0] = '\0';
    return;
  }
  // AN EINER ZEICHENGRENZE KUERZEN, NICHT NACH BYTES. Boulder schickt den
  // Namen ungekuerzt; schnitte die Uhr ein "â" oder ein kyrillisches Zeichen
  // in der Mitte durch, zeichnete sie die ganze Zeile nicht. Gelesen wird
  // hoechstens die Laenge des Tupels - fehlt die abschliessende Null, liefe
  // es sonst ueber dessen Ende hinaus.
  const size_t grenze = MIN(len, (size_t)t->length);
  const size_t n = utf8_get_size_truncate(t->value->cstring, grenze);
  memcpy(ziel, t->value->cstring, n);
  ziel[n] = '\0';
  prv_utf8_reparieren(ziel);
}

/**
 * Eine Zahl, gleich wie breit und ob mit Vorzeichen - nach dem Muster von
 * Kieselsport (telefon.c, prv_zahl). Boulder schickt int32; ein anderer
 * Sender darf auch kuerzer oder ohne Vorzeichen schicken, ohne dass die Uhr
 * vier Byte liest, wo eines liegt. false: kein Schluessel oder keine Zahl.
 */
static bool prv_zahl(DictionaryIterator *iter, uint32_t key, int32_t *aus) {
  Tuple *t = dict_find(iter, key);
  if (!t) return false;
  if (t->type == TUPLE_INT) {
    switch (t->length) {
      case 1: *aus = t->value->int8; return true;
      case 2: *aus = t->value->int16; return true;
      case 4: *aus = t->value->int32; return true;
      default: return false;
    }
  }
  if (t->type == TUPLE_UINT) {
    switch (t->length) {
      case 1: *aus = t->value->uint8; return true;
      case 2: *aus = t->value->uint16; return true;
      case 4:
        if (t->value->uint32 > INT32_MAX) return false;
        *aus = (int32_t)t->value->uint32;
        return true;
      default: return false;
    }
  }
  return false;
}

/**
 * Ein Zeitpunkt in UTC-Sekunden: die vier Byte als vorzeichenlose Zahl, gleich
 * ob als TUPLE_INT oder TUPLE_UINT geschickt. Boulder schickt TUPLE_INT mit den
 * Bits der vorzeichenlosen Zahl - ab 2038 waere das als int32 gelesen negativ,
 * und die Ankunft verschwaende. Ein Zeitpunkt in weniger als vier Byte ist
 * keiner. false: kein Schluessel oder kein Zeitpunkt.
 */
static bool prv_zeitpunkt(DictionaryIterator *iter, uint32_t key, uint32_t *aus) {
  Tuple *t = dict_find(iter, key);
  if (!t || (t->type != TUPLE_INT && t->type != TUPLE_UINT) || t->length != 4) return false;
  *aus = t->value->uint32;
  return true;
}

// Gleiche Art, aber eine andere Abzweigung: die Lage auf der Route springt
// (KST_LAGE_M) - oder, ohne Lage, die Entfernung deutlich nach oben (KST_NAH_M).
static bool prv_naechster_gleicher(int32_t vorher_m, int32_t jetzt_m, int32_t lage_vorher,
                                   int32_t lage_jetzt) {
  if (lage_vorher >= 0 && lage_jetzt >= 0) {
    const int32_t sprung = lage_jetzt - lage_vorher;
    return sprung > KST_LAGE_M || sprung < -KST_LAGE_M;
  }
  if (vorher_m < 0 || jetzt_m < 0) return false;
  if (jetzt_m - vorher_m >= KST_WEIT_M) return true;
  return vorher_m <= KST_NAH_M && jetzt_m - vorher_m >= KST_SPRUNG_M;
}

bool weg_uebernimm(DictionaryIterator *iter) {
  const int vorher = s_weg.abbiegeart;
  const int32_t vorher_m = s_weg.entfernung_m;
  int32_t zahl = 0;

  if (prv_zahl(iter, MESSAGE_KEY_ABBIEGEART, &zahl)) {
    s_weg.abbiegeart = zahl;
    // Jede Abbiegeart kommt mit ihrer eigenen Ausfahrt - oder ohne. Die Nummer
    // des vorigen Kreisels an einer gewoehnlichen Kreuzung waere eine Luege.
    s_ausfahrt = 0;
    s_winkel = WEG_KEIN_WINKEL;
  }

  if (prv_zahl(iter, MESSAGE_KEY_AUSFAHRT, &zahl)) s_ausfahrt = zahl;

  if (prv_zahl(iter, MESSAGE_KEY_WINKEL, &zahl) && zahl >= -180 && zahl <= 180) {
    s_winkel = zahl;
  }

  const bool mit_entfernung = prv_zahl(iter, MESSAGE_KEY_ENTFERNUNG, &zahl);
  if (mit_entfernung) s_weg.entfernung_m = prv_meter(zahl);

  const bool mit_rest = prv_zahl(iter, MESSAGE_KEY_REST, &zahl);
  if (mit_rest) s_weg.rest_m = prv_meter(zahl);

  // DIE LAGE NUR AUS BEIDEN ZAHLEN DIESER NACHRICHT: ein REST von vorhin
  // passte nicht zur Entfernung von jetzt.
  const int32_t lage_vorher = s_lage_m;
  s_lage_m = (mit_entfernung && mit_rest && s_weg.entfernung_m >= 0 &&
              s_weg.rest_m >= s_weg.entfernung_m)
                 ? s_weg.rest_m - s_weg.entfernung_m : -1;

  uint32_t zeitpunkt = 0;
  if (prv_zeitpunkt(iter, MESSAGE_KEY_ANKUNFT, &zeitpunkt)) s_weg.ankunft = zeitpunkt;

  prv_nimm_text(iter, MESSAGE_KEY_STRASSE, s_weg.strasse, KST_STRASSE_LEN);

  s_weg.empfangen = time(NULL);
  s_geaendert = true;

  // NEU heisst: eine andere Abbiegeart - oder dieselbe noch einmal an einer
  // anderen Abzweigung (prv_naechster_gleicher). Eine schrumpfende Entfernung
  // ist derselbe Schritt und kein Anlass zu summen.
  const bool neu = s_weg.abbiegeart > 0 &&
                   (s_weg.abbiegeart != vorher ||
                    prv_naechster_gleicher(vorher_m, s_weg.entfernung_m, lage_vorher, s_lage_m));
  // Nur ein neuer Schritt kommt sofort in den Persist (siehe weg_sichern) -
  // stuerzt die App ab, steht beim naechsten Oeffnen wenigstens die Richtung.
  if (neu) weg_sichern();
  APP_LOG(APP_LOG_LEVEL_INFO, "Art %d -> %d, %d m : %s",
          vorher, s_weg.abbiegeart, (int)s_weg.entfernung_m,
          neu ? "NEUER SCHRITT" : "derselbe");
  return neu;
}

void weg_sichern(void) {
  // Nie einen leeren Anfangsstand schreiben: der wuerde den letzten echten
  // Stand ueberschreiben, ohne etwas Neues zu wissen.
  if (s_weg.empfangen == 0) return;
  // Unveraendert nicht noch einmal: settings_file_set vergleicht nicht, es
  // schreibt jedes Mal. So bleibt es bei einem Vorgang je neuem Schritt und
  // hoechstens einem beim Beenden, wenn danach noch etwas kam.
  if (!s_geaendert) return;
  if (persist_write_data(PERSIST_WEG, &s_weg, sizeof(s_weg)) == (int)sizeof(s_weg)) {
    s_geaendert = false;
  }
}

const Weg *weg_stand(void) { return &s_weg; }

bool weg_hat_zahl(void) { return s_weg.entfernung_m >= 0; }

bool weg_veraltet(void) {
  if (s_weg.empfangen == 0) return true;
  return (time(NULL) - s_weg.empfangen) > KST_VERALTET_S;
}

void weg_alter_text(char *buf, size_t len) {
  if (s_weg.empfangen == 0) {
    snprintf(buf, len, "%s", ST(STR_ALTER_NIE));
    return;
  }
  const int32_t s = (int32_t)(time(NULL) - s_weg.empfangen);
  if (s < 60) snprintf(buf, len, "%s", ST(STR_ALTER_JETZT));
  else if (s < 3600) snprintf(buf, len, ST(STR_ALTER_MIN), (int)(s / 60));
  else if (s < 86400) snprintf(buf, len, ST(STR_ALTER_STD), (int)(s / 3600));
  else snprintf(buf, len, "%s", ST(STR_ALTER_GESTERN));
}

int weg_balken_prozent(void) {
  if (s_weg.entfernung_m < 0 || s_weg.entfernung_m > KST_BALKEN_AB_M) return -1;
  // Laeuft LEER, je naeher die Abzweigung kommt.
  const int32_t p = s_weg.entfernung_m * 100 / KST_BALKEN_AB_M;
  return (int)(p < 0 ? 0 : (p > 100 ? 100 : p));
}

int weg_ausfahrt(void) { return s_ausfahrt; }

int weg_winkel(void) { return s_winkel; }
