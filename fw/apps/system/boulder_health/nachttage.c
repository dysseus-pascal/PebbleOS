/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#ifdef CONFIG_BOULDER_HEALTH_APP

#include "daten.h"

#include <string.h>

// Eigene Datei, weil sie nur rechnet: Karten und Details brauchen sie, ohne daten.c und dessen
// Syscalls zu binden (test_bh_karten zeichnet ohne sie; dort steht bh_ortsversatz als Attrappe).

#define S_JE_TAG       (24 * 60 * 60)
#define TAGE_1970_2000 10957

bool bh_nacht_da(const BhDaten *d) {
  return d->nacht_lesen == BoulderLesenDa && d->nacht.anzahl > 0;
}

static int64_t prv_ort(const NachtBild *n, uint16_t minute) {
  const time_t utc = (time_t)n->beginn_utc + (time_t)minute * 60;
  return (int64_t)utc + bh_ortsversatz(utc);
}

uint16_t bh_nacht_wachtag(const NachtBild *n) {
  return (uint16_t)(prv_ort(n, n->aufwachen) / S_JE_TAG - TAGE_1970_2000);
}

uint16_t bh_nacht_uhrzeit(const NachtBild *n, uint16_t minute) {
  return (uint16_t)((prv_ort(n, minute) % S_JE_TAG) / 60);
}

uint8_t bh_daten_nachttage(const BhDaten *d, BhNachtTag aus[NAECHTE_MAX]) {
  uint8_t anzahl = 0;
  uint16_t min_ruhe = 0, min_hrv = 0;
  if (d->naechte_da && d->naechte.neu < NAECHTE_MAX) {
    anzahl = (d->naechte.anzahl <= NAECHTE_MAX) ? d->naechte.anzahl : NAECHTE_MAX;
    // Ein Ring: tage[neu] ist der juengste Wachtag, die aelteren stehen rueckwaerts davor.
    for (uint8_t i = 0; i < anzahl; i++) {
      const uint8_t platz = (uint8_t)((d->naechte.neu + NAECHTE_MAX - i) % NAECHTE_MAX);
      const BoulderNachtEintrag *e = &d->naechte.tage[platz];
      aus[i] = (BhNachtTag){.tag = e->tag, .hrv = e->hrv, .ruhepuls = e->ruhepuls};
    }
    min_ruhe = d->naechte.min_ruhe;
    min_hrv = d->naechte.min_hrv;
  }
  if (!bh_nacht_da(d)) {
    return anzahl;
  }
  const NachtBild *n = &d->nacht;
  const uint16_t tag = bh_nacht_wachtag(n);
  if (anzahl == 0 || tag > aus[0].tag) {
    // Ein neuer juengster Wachtag; ist die Liste voll, faellt der aelteste.
    const uint8_t bleiben = (anzahl < NAECHTE_MAX) ? anzahl : NAECHTE_MAX - 1;
    memmove(&aus[1], &aus[0], bleiben * sizeof(aus[0]));
    aus[0] = (BhNachtTag){.tag = tag, .hrv = n->hrv, .ruhepuls = n->ruhepuls};
    return bleiben + 1;
  }
  if (tag == aus[0].tag) {
    // Noch eine Nacht des juengsten Tags: die laengere gilt, bei gleicher Laenge die fruehere,
    // die schon in der Liste steht.
    if (n->schlaf_minuten > min_ruhe) {
      aus[0].ruhepuls = n->ruhepuls;
    }
    if (n->hrv && n->schlaf_minuten > min_hrv) {
      aus[0].hrv = n->hrv;
    }
  }
  return anzahl;
}

bool bh_daten_schlaf(const BhDaten *d, uint16_t tag, uint16_t *minuten, bool *fertig) {
  *minuten = 0;
  *fertig = false;
  // Die Liste kennt die Minuten nur fuer ihren juengsten Tag (min_ruhe, laengste Nacht).
  if (d->naechte_da && d->naechte.anzahl > 0 && d->naechte.neu < NAECHTE_MAX &&
      d->naechte.tage[d->naechte.neu].tag == tag) {
    *minuten = d->naechte.min_ruhe;
    *fertig = true;
  }
  if (bh_nacht_da(d) && bh_nacht_wachtag(&d->nacht) == tag && d->nacht.schlaf_minuten > *minuten) {
    *minuten = d->nacht.schlaf_minuten;
    *fertig = d->nacht.fertig;
  }
  return *minuten > 0;
}

#endif // CONFIG_BOULDER_HEALTH_APP
