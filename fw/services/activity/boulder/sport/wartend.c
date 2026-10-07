/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "wartend.h"
#include "schluessel.h"

#define HALB 200

// Texte wie persist_write_string: mit der abschliessenden Null, damit die
// Store-App dieselben Eintraege lesen kann.
static void prv_text_schreiben(uint32_t key, const char *text) {
  ks_persist_write_data(key, text, strlen(text) + 1);
}

static void prv_text_lesen(uint32_t key, char *aus, size_t platz) {
  if (platz == 0) {
    return;
  }
  const int gelesen = ks_persist_read_data(key, aus, platz);
  aus[gelesen > 0 ? (size_t)gelesen - 1 : 0] = '\0';
}

// PAUSEN UND ENDE TRAGEN DEN BEGINN IHRES TRAININGS. Aeltere Firmware kennt
// die Schluessel nicht und loescht sie nach der Bestaetigung nicht mit; nach
// einem Zurueckspielen und neuem Aufspielen laegen sonst die Pausen eines
// frueheren Trainings neben einer fremden Zusammenfassung.
typedef struct {
  uint32_t beginn;
  uint32_t ende;
} Bezug;

// true, wenn Pausen und Ende zum wartenden Stand gehoeren.
static bool prv_bezug(Bezug *b) {
  Trainingsstand t = { 0 };
  if (!ks_persist_exists(PERSIST_WARTET) || !ks_persist_exists(PERSIST_WARTET_ENDE) ||
      ks_persist_read_data(PERSIST_WARTET_ENDE, b, sizeof(*b)) != (int)sizeof(*b)) {
    return false;
  }
  ks_persist_read_data(PERSIST_WARTET, &t, sizeof(t));
  return b->beginn != 0 && b->beginn == t.beginn;
}

void wartend_merken(const Trainingsstand *t, const char *liste, const uint8_t *pausen,
                    size_t pausen_laenge, uint32_t ende) {
  // Erst Pausen und Ende, dann der Stand: ohne den Stand gilt nichts als
  // wartend, und fiele dazwischen der Strom aus, passte der Beginn nicht.
  if (pausen && pausen_laenge > 0) {
    ks_persist_write_data(PERSIST_WARTET_PAUSEN, pausen, pausen_laenge);
  } else {
    ks_persist_delete(PERSIST_WARTET_PAUSEN);
  }
  const Bezug b = { .beginn = t->beginn, .ende = ende };
  ks_persist_write_data(PERSIST_WARTET_ENDE, &b, sizeof(b));
  ks_persist_write_data(PERSIST_WARTET, t, sizeof(*t));
  char halb[HALB + 1];
  strncpy(halb, liste, HALB);
  halb[HALB] = 0;
  prv_text_schreiben(PERSIST_WARTET_LISTE_A, halb);
  if (strlen(liste) > HALB) {
    prv_text_schreiben(PERSIST_WARTET_LISTE_B, liste + HALB);
  } else {
    ks_persist_delete(PERSIST_WARTET_LISTE_B);
  }
}

bool wartend_laden(Trainingsstand *t, char *liste, size_t platz) {
  if (!ks_persist_exists(PERSIST_WARTET)) {
    return false;
  }
  memset(t, 0, sizeof(*t));
  ks_persist_read_data(PERSIST_WARTET, t, sizeof(*t));
  liste[0] = 0;
  if (ks_persist_exists(PERSIST_WARTET_LISTE_A)) {
    prv_text_lesen(PERSIST_WARTET_LISTE_A, liste, platz > HALB + 1 ? HALB + 1 : platz);
  }
  if (ks_persist_exists(PERSIST_WARTET_LISTE_B)) {
    const size_t bisher = strlen(liste);
    if (platz > bisher + 1) {
      prv_text_lesen(PERSIST_WARTET_LISTE_B, liste + bisher, platz - bisher);
    }
  }
  return t->beginn > 0 && t->dauer_s > 0;
}

size_t wartend_pausen(uint8_t *aus, size_t platz) {
  Bezug b;
  if (!prv_bezug(&b) || !ks_persist_exists(PERSIST_WARTET_PAUSEN)) {
    return 0;
  }
  const int gelesen = ks_persist_read_data(PERSIST_WARTET_PAUSEN, aus, platz);
  return gelesen > 0 ? (size_t)gelesen : 0;
}

uint32_t wartend_ende(void) {
  Bezug b;
  return prv_bezug(&b) ? b.ende : 0;
}

void wartend_vergessen(uint32_t beginn) {
  // EINE SPAET BESTAETIGTE ALTE ZUSAMMENFASSUNG loescht keine neuere - wie
  // beim Stopp. Inzwischen kann ein zweites Training ihren Platz haben.
  // Laesst sie sich nicht lesen, bleibt sie: doppelt angekommen schadet
  // nicht, das Telefon kennt sie am Beginn.
  if (beginn != 0 && ks_persist_exists(PERSIST_WARTET)) {
    Trainingsstand t = { 0 };
    if (ks_persist_read_data(PERSIST_WARTET, &t, sizeof(t)) <= 0 || t.beginn != beginn) {
      return;
    }
  }
  ks_persist_delete(PERSIST_WARTET);
  ks_persist_delete(PERSIST_WARTET_LISTE_A);
  ks_persist_delete(PERSIST_WARTET_LISTE_B);
  ks_persist_delete(PERSIST_WARTET_PAUSEN);
  ks_persist_delete(PERSIST_WARTET_ENDE);
}

bool wartend_da(void) {
  return ks_persist_exists(PERSIST_WARTET);
}

// --- Stopp ---

typedef struct {
  uint32_t beginn;
  uint32_t zeit;
  uint8_t art;
} Stopp;

void stopp_merken(uint32_t beginn, uint32_t zeit, uint8_t art) {
  const Stopp s = { .beginn = beginn, .zeit = zeit, .art = art };
  ks_persist_write_data(PERSIST_STOPP, &s, sizeof(s));
}

bool stopp_laden(uint32_t *beginn, uint32_t *zeit, uint8_t *art) {
  Stopp s = { 0 };
  if (!ks_persist_exists(PERSIST_STOPP) ||
      ks_persist_read_data(PERSIST_STOPP, &s, sizeof(s)) != (int)sizeof(s) || s.beginn == 0) {
    return false;
  }
  *beginn = s.beginn;
  *zeit = s.zeit;
  *art = s.art;
  return true;
}

void stopp_vergessen(uint32_t beginn) {
  uint32_t b = 0, z = 0;
  uint8_t a = 0;
  // EIN SPAETER BESTAETIGTER ALTER STOPP loescht keinen neueren.
  if (beginn != 0 && stopp_laden(&b, &z, &a) && b != beginn) {
    return;
  }
  ks_persist_delete(PERSIST_STOPP);
}

bool stopp_da(void) {
  return ks_persist_exists(PERSIST_STOPP);
}
