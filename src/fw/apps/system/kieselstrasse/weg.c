/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pebble_compat.h"
#include "schluessel.h"
#include "texte.h"
#include "weg.h"

// Ab wann ein Stand keine Navigation mehr ist. Zehn Minuten sind lang genug
// fuer einen Stau und kurz genug, dass niemand einem Pfeil von gestern folgt.
#define KST_VERALTET_S 600

// Unter dieser Entfernung laeuft der Balken - die Spanne, in der ein Blick
// aufs Handgelenk noch etwas aendert.
#define KST_BALKEN_AB_M 300

static Weg s_weg;

// Ausfahrt und Winkel stehen neben s_weg, nicht darin (siehe weg.h).
static int s_ausfahrt;
static int s_winkel;

// Weicht der Stand vom Persist ab? Nur dann lohnt ein Schreibvorgang - ein
// Blick auf die Uhr ohne eine einzige Nachricht soll den Flash nicht beruehren.
static bool s_geaendert;

static void prv_anfang(void) {
  memset(&s_weg, 0, sizeof(s_weg));
  s_weg.entfernung_m = -1;
  s_weg.rest_m = -1;
  s_ausfahrt = 0;
  s_winkel = WEG_KEIN_WINKEL;
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
  const char *neu = (t->length <= 1) ? "" : t->value->cstring;
  strncpy(ziel, neu, len - 1);
  ziel[len - 1] = '\0';
}

bool weg_uebernimm(DictionaryIterator *iter) {
  const int vorher = s_weg.abbiegeart;

  Tuple *a = dict_find(iter, MESSAGE_KEY_ABBIEGEART);
  if (a && a->type == TUPLE_INT) {
    s_weg.abbiegeart = a->value->int32;
    // Jede Abbiegeart kommt mit ihrer eigenen Ausfahrt - oder ohne. Die Nummer
    // des vorigen Kreisels an einer gewoehnlichen Kreuzung waere eine Luege.
    s_ausfahrt = 0;
    s_winkel = WEG_KEIN_WINKEL;
  }

  Tuple *nr = dict_find(iter, MESSAGE_KEY_AUSFAHRT);
  if (nr && nr->type == TUPLE_INT) s_ausfahrt = nr->value->int32;

  Tuple *wi = dict_find(iter, MESSAGE_KEY_WINKEL);
  if (wi && wi->type == TUPLE_INT && wi->value->int32 >= -180 && wi->value->int32 <= 180) {
    s_winkel = wi->value->int32;
  }

  Tuple *e = dict_find(iter, MESSAGE_KEY_ENTFERNUNG);
  if (e && e->type == TUPLE_INT) s_weg.entfernung_m = e->value->int32;

  Tuple *r = dict_find(iter, MESSAGE_KEY_REST);
  if (r && r->type == TUPLE_INT) s_weg.rest_m = r->value->int32;

  Tuple *k = dict_find(iter, MESSAGE_KEY_ANKUNFT);
  if (k && k->type == TUPLE_INT) s_weg.ankunft = (time_t)k->value->int32;

  prv_nimm_text(iter, MESSAGE_KEY_STRASSE, s_weg.strasse, KST_STRASSE_LEN);

  s_weg.empfangen = time(NULL);
  s_geaendert = true;

  // NEU heisst: eine andere Abbiegeart. Eine schrumpfende Entfernung ist
  // derselbe Schritt und kein Anlass zu summen.
  const bool neu = (s_weg.abbiegeart != vorher) && s_weg.abbiegeart > 0;
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
