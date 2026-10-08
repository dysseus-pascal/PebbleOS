/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// Boulder V2, Schritt 3: die Schritte der Vortage (woche.h, Spezifikation 3.8.3, AP4). Nur mit der
// eigenen Health-App; ohne den Schalter bleibt die Datei leer.
#ifdef CONFIG_BOULDER_HEALTH_APP

#include "pbl/services/activity/boulder/woche.h"

#include "pbl/kernel/mutex.h"
#include "pbl/services/activity/activity.h"
#include "pbl/services/activity/boulder/speicher.h"
#include "pbl/services/time.h"
#include "pbl/util/testing.h"
#include "pbl/util/units.h"
#include <pbl/logging/logging.h>

#include <string.h>

PBL_LOG_MODULE_DECLARE(service_activity, CONFIG_SERVICE_ACTIVITY_LOG_LEVEL);

static const char SCHLUESSEL[] = "woche";
// Ohne Nullbyte, wie "ruhe", "kopf" und "naechte" (speicher.c).
#define SCHLUESSEL_LEN (sizeof(SCHLUESSEL) - 1)

// 1.1.2000 0:00 als Ortszeit in Sekunden seit 1970: der Tag 0 (wie naechte.c und die App).
#define TAG_NULL_LOKAL 946684800

typedef struct {
  BoulderWoche woche;
  BoulderSchrittSpiegel spiegel;
} WocheSatz;

_Static_assert(sizeof(WocheSatz) == 36, "woche: 28 B Woche und 8 B Spiegel (3.5)");

#define WOCHE_LEER                                                                        \
  ((BoulderWoche){ .schritte = { WOCHE_KEIN_WERT, WOCHE_KEIN_WERT, WOCHE_KEIN_WERT,       \
                                 WOCHE_KEIN_WERT, WOCHE_KEIN_WERT, WOCHE_KEIN_WERT } })

// EINE BLATTSPERRE (3.4): Unter ihr wird nur kopiert, nichts gerufen. Fuer die Tests sichtbar
// (Sperrfolge, T2.4). Nur KernelBG schreibt die Woche; der Spiegel gehoert KernelBG allein.
PBL_T_STATIC PBL_MUTEX_DEFINE(s_woche_sperre);
static BoulderWoche s_woche = WOCHE_LEER;
static BoulderSchrittSpiegel s_spiegel;
static bool s_geladen;          //< "Laden" hat die Woche seit dem Boot gelesen (unter der Sperre)
static bool s_geaendert;        //< seit dem letzten Satz geaendert (nur KernelBG)
static bool s_tageswechsel;     //< die naechste Ablage schreibt, ohne die Stunde abzuwarten
static time_t s_geschrieben;    //< Zeit des letzten Satzes seit dem Boot, 0 = noch keiner

// Tage seit dem 1.1.2000 nach Ortszeit, mit der Zeitzone samt Sommerzeit dieser Minute (wie
// Pebbles time_util_get_day, nur ab 2000). 0 ohne gueltige Uhrzeit.
static uint16_t prv_tag(time_t utc) {
  const time_t lokal = time_utc_to_local(utc);
  if (lokal < TAG_NULL_LOKAL + PBL_SEC_PER_DAY) {
    return 0;
  }
  const uint32_t tage = (uint32_t)((lokal - TAG_NULL_LOKAL) / PBL_SEC_PER_DAY);
  return (tage > UINT16_MAX) ? UINT16_MAX : (uint16_t)tage;
}

// Die Woche mit `heute - 1` als juengstem Vortag: der Spiegel, wenn er dieser Tag ist, sonst was
// die alte Woche zu dem Tag weiss.
static BoulderWoche prv_weiterruecken(const BoulderWoche *alt, const BoulderSchrittSpiegel *spiegel,
                                      uint16_t heute) {
  BoulderWoche neu = WOCHE_LEER;
  neu.tag = (uint16_t)(heute - 1);
  for (int i = 0; i < WOCHE_VORTAGE; i++) {
    const int tag = (int)neu.tag - i;
    const int alt_i = (int)alt->tag - tag;
    if (tag == spiegel->tag) {
      neu.schritte[i] = spiegel->schritte;
    } else if (alt->tag && alt_i >= 0 && alt_i < WOCHE_VORTAGE) {
      neu.schritte[i] = alt->schritte[alt_i];
    }
  }
  return neu;
}

void boulder_woche_minute(time_t minute_utc) {
  // Vor "Laden" nichts: Der Spiegel kaeme sonst vor dem aus der Datei und wuerde ueberschrieben.
  // Pebbles Wert ist ein Tagesstand; die naechste Minute nach "Laden" holt ihn ganz.
  if (!s_geladen) {
    return;
  }
  const uint16_t tag = prv_tag(minute_utc);
  int32_t schritte = 0;
  // PEBBLES METRIK IM RAM, Verlauf 1 (keine Datei). Ausserhalb jeder Sperre der Variante: Sie nimmt
  // die Activity-Sperre, und die kommt vor der Variante (variante.h).
  if (!tag || !activity_get_metric(ActivityMetricStepCount, 1, &schritte) || schritte < 0) {
    return;
  }
  if (tag == s_spiegel.tag) {
    if (s_spiegel.schritte != (uint32_t)schritte) {
      s_spiegel.schritte = (uint32_t)schritte;
      s_geaendert = true;
    }
    return;
  }
  if (s_spiegel.tag && tag > s_spiegel.tag) {
    // Der Spiegel ist fertig: Pebbles Wert des Vortags, mit dessen letzter Minute.
    const BoulderWoche neu = prv_weiterruecken(&s_woche, &s_spiegel, tag);
    pbl_mutex_lock(&s_woche_sperre, PBL_FOREVER);
    s_woche = neu;
    pbl_mutex_unlock(&s_woche_sperre);
    s_tageswechsel = true;
    PBL_LOG_INFO("Boulder: Woche bis Tag %d, Vortag %lu Schritte", (int)neu.tag,
                 (unsigned long)neu.schritte[0]);
    boulder_speicher_anstossen(false);
  }
  s_spiegel = (BoulderSchrittSpiegel){ .tag = tag, .schritte = (uint32_t)schritte };
  s_geaendert = true;
}

bool boulder_woche_kopieren(BoulderWoche *aus) {
  // UNTER DER SPERRE NUR DIE KOPIE (3.1, Regel 3), direkt in den Puffer des Aufrufers.
  pbl_mutex_lock(&s_woche_sperre, PBL_FOREVER);
  const bool geladen = s_geladen;
  *aus = geladen ? s_woche : WOCHE_LEER;
  pbl_mutex_unlock(&s_woche_sperre);
  return geladen;
}

void boulder_woche_laden(SettingsFile *datei) {
  WocheSatz satz;
  const int laenge = datei ? settings_file_get_len(datei, SCHLUESSEL, SCHLUESSEL_LEN) : 0;
  const bool gelesen =
      laenge == (int)sizeof(satz) &&
      settings_file_get(datei, SCHLUESSEL, SCHLUESSEL_LEN, &satz, sizeof(satz)) == S_SUCCESS;
  if (!gelesen) {
    if (laenge > 0) {
      PBL_LOG_WRN("Boulder: Woche in %s ungueltig (%d B), sie beginnt leer", SPEICHER_DATEI,
                  laenge);
    }
    satz = (WocheSatz){ .woche = WOCHE_LEER };
  }
  s_spiegel = satz.spiegel;
  s_geaendert = false;
  s_tageswechsel = false;
  s_geschrieben = 0;
  pbl_mutex_lock(&s_woche_sperre, PBL_FOREVER);
  s_woche = satz.woche;
  s_geladen = true;
  pbl_mutex_unlock(&s_woche_sperre);
  PBL_LOG_INFO("Boulder: Woche bis Tag %d, Spiegel Tag %d mit %lu Schritten", (int)s_woche.tag,
               (int)s_spiegel.tag, (unsigned long)s_spiegel.schritte);
}

void boulder_woche_ablegen(SettingsFile *datei, time_t jetzt) {
  if (!s_geladen || !s_geaendert) {
    return;
  }
  const bool stunde_um = !s_geschrieben || jetzt - s_geschrieben >= WOCHE_ABLAGE_ABSTAND_S ||
                         jetzt < s_geschrieben;
  if (!s_tageswechsel && !stunde_um) {
    return;
  }
  // Ohne Kopie unter der Sperre: Die Ablage laeuft auf KernelBG, und nur KernelBG aendert beides.
  const WocheSatz satz = { .woche = s_woche, .spiegel = s_spiegel };
  if (settings_file_set(datei, SCHLUESSEL, SCHLUESSEL_LEN, &satz, sizeof(satz)) != S_SUCCESS) {
    PBL_LOG_WRN("Boulder: Woche nicht gespeichert, naechste Ablage wieder");
    return;
  }
  s_geaendert = false;
  s_tageswechsel = false;
  s_geschrieben = jetzt;
}

const BoulderWoche *boulder_woche_kern(void) {
  return &s_woche;
}

const BoulderSchrittSpiegel *boulder_woche_spiegel(void) {
  return &s_spiegel;
}

void boulder_woche_test_neustart(void) {
  s_woche = WOCHE_LEER;
  s_spiegel = (BoulderSchrittSpiegel){ 0 };
  s_geladen = false;
  s_geaendert = false;
  s_tageswechsel = false;
  s_geschrieben = 0;
}

#endif // CONFIG_BOULDER_HEALTH_APP
