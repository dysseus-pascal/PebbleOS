/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#ifdef CONFIG_BOULDER_HEALTH_APP

#include "daten.h"

// Eigene Datei, weil sie nur rechnet: Karten und Details brauchen sie, ohne daten.c und dessen
// Syscalls zu binden (test_bh_karten zeichnet ohne sie).

uint8_t bh_daten_nachttage(const BhDaten *d, BhNachtTag aus[NAECHTE_MAX]) {
  if (!d->naechte_da) {
    return 0;
  }
  const uint8_t anzahl = (d->naechte.anzahl <= NAECHTE_MAX) ? d->naechte.anzahl : NAECHTE_MAX;
  if (d->naechte.neu >= NAECHTE_MAX) {
    return 0;
  }
  // Ein Ring: tage[neu] ist der juengste Wachtag, die aelteren stehen rueckwaerts davor.
  for (uint8_t i = 0; i < anzahl; i++) {
    const uint8_t platz = (uint8_t)((d->naechte.neu + NAECHTE_MAX - i) % NAECHTE_MAX);
    const BoulderNachtEintrag *e = &d->naechte.tage[platz];
    aus[i] = (BhNachtTag){.tag = e->tag, .hrv = e->hrv, .ruhepuls = e->ruhepuls};
  }
  return anzahl;
}

#endif // CONFIG_BOULDER_HEALTH_APP
