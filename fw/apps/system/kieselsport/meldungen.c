/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// Nur mit der Variante: fw/apps/system/ baut jedes Board (siehe kieselsport.c).
#ifdef CONFIG_BOULDER_KIESELSPORT

#include "meldungen.h"

#include <string.h>

static Meldung *prv_an(Meldungen *m, uint8_t i) {
  return &m->platz[(m->kopf + i) % MELDUNGEN_MAX];
}

static const Meldung *prv_an_c(const Meldungen *m, uint8_t i) {
  return &m->platz[(m->kopf + i) % MELDUNGEN_MAX];
}

void meldungen_leeren(Meldungen *m) {
  memset(m, 0, sizeof(*m));
}

bool meldungen_leer(const Meldungen *m) { return m->anzahl == 0; }

uint8_t meldungen_anzahl(const Meldungen *m) { return m->anzahl; }

// Die Meldung an Stelle [i] herausnehmen, die dahinter ruecken auf.
static void prv_entferne(Meldungen *m, uint8_t i) {
  for (uint8_t k = i; k + 1 < m->anzahl; k++) {
    *prv_an(m, k) = *prv_an(m, (uint8_t)(k + 1));
  }
  m->anzahl--;
}

void meldungen_anstellen(Meldungen *m, Meldung neu) {
  if (m->anzahl >= MELDUNGEN_MAX) {
    // DIE AELTESTE FAELLT WEG - ausser sie ist gerade unterwegs: deren
    // Bestaetigung gehoerte sonst der falschen.
    if (m->unterwegs) {
      prv_entferne(m, 1);
    } else {
      meldungen_vorn_weg(m);
    }
  }
  *prv_an(m, m->anzahl) = neu;
  m->anzahl++;
}

bool meldungen_hat_stopp(const Meldungen *m, uint32_t beginn) {
  for (uint8_t i = 0; i < m->anzahl; i++) {
    const Meldung *e = prv_an_c(m, i);
    if (e->aus_persist && e->beginn == beginn) return true;
  }
  return false;
}

bool meldungen_fuer(const Meldungen *m, uint32_t beginn) {
  for (uint8_t i = 0; i < m->anzahl; i++) {
    if (prv_an_c(m, i)->beginn == beginn) return true;
  }
  return false;
}

const Meldung *meldungen_vorn(const Meldungen *m) {
  return m->anzahl ? prv_an_c(m, 0) : NULL;
}

bool meldungen_anlauf(Meldungen *m, uint8_t hoechstens) {
  if (!m->anzahl) return false;
  if (m->anlaeufe >= hoechstens) {
    meldungen_vorn_weg(m);
    return false;
  }
  m->anlaeufe++;
  return true;
}

void meldungen_vorn_weg(Meldungen *m) {
  if (!m->anzahl) return;
  m->kopf = (uint8_t)((m->kopf + 1) % MELDUNGEN_MAX);
  m->anzahl--;
  m->anlaeufe = 0;
  m->unterwegs = false;
}

#endif  // CONFIG_BOULDER_KIESELSPORT
