/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "apps/system/kieselsport/meldungen.h"

// Die Zustandsmeldungen der App: jede kommt an, in ihrer Reihenfolge, mit
// ihrer Uhrzeit - statt dass eine neue die offene ersetzt.

#define PAUSE 2
#define WEITER 3
#define STOPP 0

static Meldungen s_m;

static Meldung prv_meldung(uint8_t was, uint32_t zeit) {
  return (Meldung){ .was = was, .art = 1, .beginn = 1000, .zeit = zeit };
}

void test_kieselsport_meldungen__initialize(void) {
  meldungen_leeren(&s_m);
}

void test_kieselsport_meldungen__schnelles_pause_weiter_verdraengt_nichts(void) {
  meldungen_anstellen(&s_m, prv_meldung(PAUSE, 1100));
  meldungen_anstellen(&s_m, prv_meldung(WEITER, 1102));
  meldungen_anstellen(&s_m, prv_meldung(PAUSE, 1200));
  cl_assert_equal_i(meldungen_anzahl(&s_m), 3);
  const uint8_t erwartet[] = { PAUSE, WEITER, PAUSE };
  const uint32_t zeiten[] = { 1100, 1102, 1200 };
  for (int i = 0; i < 3; i++) {
    cl_assert(meldungen_anlauf(&s_m, 6));
    cl_assert_equal_i(meldungen_vorn(&s_m)->was, erwartet[i]);
    cl_assert_equal_i(meldungen_vorn(&s_m)->zeit, zeiten[i]);
    meldungen_vorn_weg(&s_m);
  }
  cl_assert(meldungen_leer(&s_m));
  cl_assert(meldungen_vorn(&s_m) == NULL);
}

void test_kieselsport_meldungen__nach_sechs_anlaeufen_die_naechste(void) {
  meldungen_anstellen(&s_m, prv_meldung(PAUSE, 1100));
  meldungen_anstellen(&s_m, prv_meldung(WEITER, 1150));
  for (int i = 0; i < 6; i++) {
    cl_assert(meldungen_anlauf(&s_m, 6));
  }
  // Der siebte Anlauf gibt die Pause auf - das Weiter ist vorn, mit frischen Anlaeufen.
  cl_assert(!meldungen_anlauf(&s_m, 6));
  cl_assert_equal_i(meldungen_vorn(&s_m)->was, WEITER);
  cl_assert(meldungen_anlauf(&s_m, 6));
  cl_assert_equal_i(s_m.anlaeufe, 1);
}

void test_kieselsport_meldungen__voll_faellt_die_aelteste_weg(void) {
  for (uint32_t i = 0; i < MELDUNGEN_MAX + 2; i++) {
    meldungen_anstellen(&s_m, prv_meldung(i % 2 ? WEITER : PAUSE, 1000 + i));
  }
  cl_assert_equal_i(meldungen_anzahl(&s_m), MELDUNGEN_MAX);
  cl_assert_equal_i(meldungen_vorn(&s_m)->zeit, 1002);
}

void test_kieselsport_meldungen__die_unterwegs_bleibt(void) {
  for (uint32_t i = 0; i < MELDUNGEN_MAX; i++) {
    meldungen_anstellen(&s_m, prv_meldung(PAUSE, 1000 + i));
  }
  // Die vordere ist beim Telefon: ihre Bestaetigung muss ihr gelten.
  cl_assert(meldungen_anlauf(&s_m, 6));
  s_m.unterwegs = true;
  meldungen_anstellen(&s_m, prv_meldung(WEITER, 2000));
  cl_assert_equal_i(meldungen_anzahl(&s_m), MELDUNGEN_MAX);
  cl_assert_equal_i(meldungen_vorn(&s_m)->zeit, 1000);
  cl_assert_equal_i(s_m.anlaeufe, 1);
  meldungen_vorn_weg(&s_m);
  // Dahinter fehlt die zweitaelteste, die juengste steht hinten.
  cl_assert_equal_i(meldungen_vorn(&s_m)->zeit, 1002);
  cl_assert(!s_m.unterwegs);
  while (meldungen_anzahl(&s_m) > 1) {
    meldungen_vorn_weg(&s_m);
  }
  cl_assert_equal_i(meldungen_vorn(&s_m)->zeit, 2000);
}

void test_kieselsport_meldungen__gemerkter_stopp_nur_einmal(void) {
  Meldung stopp = prv_meldung(STOPP, 1500);
  stopp.aus_persist = true;
  cl_assert(!meldungen_hat_stopp(&s_m, 1000));
  meldungen_anstellen(&s_m, stopp);
  cl_assert(meldungen_hat_stopp(&s_m, 1000));
  cl_assert(!meldungen_hat_stopp(&s_m, 999));
}

void test_kieselsport_meldungen__zusammenfassung_wartet_auf_ihr_training(void) {
  meldungen_anstellen(&s_m, prv_meldung(PAUSE, 1100));
  cl_assert(meldungen_fuer(&s_m, 1000));
  // Der Start eines anderen Trainings haelt sie nicht auf.
  cl_assert(!meldungen_fuer(&s_m, 3000));
  meldungen_vorn_weg(&s_m);
  cl_assert(!meldungen_fuer(&s_m, 1000));
}
