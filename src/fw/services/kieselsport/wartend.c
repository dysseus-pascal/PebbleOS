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

void wartend_merken(const Trainingsstand *t, const char *liste) {
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

void wartend_vergessen(void) {
  ks_persist_delete(PERSIST_WARTET);
  ks_persist_delete(PERSIST_WARTET_LISTE_A);
  ks_persist_delete(PERSIST_WARTET_LISTE_B);
}
