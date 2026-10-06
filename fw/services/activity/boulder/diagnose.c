/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// Boulder V2: der taegliche Diagnosesatz (diagnose.h).

#include "pbl/services/activity/boulder/diagnose.h"

#include "kernel/kernel_heap.h"
#include "pbl/services/time.h"
#include <pbl/logging/logging.h>

#include <string.h>

PBL_LOG_MODULE_DECLARE(service_activity, CONFIG_SERVICE_ACTIVITY_LOG_LEVEL);

#define DIAGNOSE_SCHLUESSEL "diag"

static BoulderDiagnoseTag s_heute;
static BoulderDiagnoseTag s_datei;  //< zuletzt geschrieben oder gelesen
static bool s_geladen;              //< erst nach "Laden" zaehlt der Tag (sonst ginge ein
                                    //< Neustart des Tages im leeren Satz verloren)

static void prv_neuer_tag(uint16_t tag) {
  s_heute = (BoulderDiagnoseTag){
    .fassung = BOULDER_DIAGNOSE_FASSUNG, .tag = tag, .heap_min = UINT32_MAX,
    .schlange_min = UINT8_MAX,
  };
  boulder_messung_tag_neu();
}

// Den Satz aus den Messungen nachfuehren: laengster Schritt mit Name, Minutenteil, Schlange.
static void prv_auffrischen(void) {
  BoulderAufgabe *const *liste;
  const int n = boulder_messung_liste(&liste);
  const BoulderAufgabe *minutenteil = boulder_messung_minutenteil();
  for (int i = 0; i < n; i++) {
    const BoulderAufgabe *a = liste[i];
    if (!a || a->tag_max_ms <= s_heute.max_ms) {
      continue;
    }
    s_heute.max_ms = a->tag_max_ms;
    const char *name = a->name;
    if (strncmp(name, "Boulder ", 8) == 0) {
      name += 8;
    }
    strncpy(s_heute.max_name, name, sizeof(s_heute.max_name) - 1);
    s_heute.max_name[sizeof(s_heute.max_name) - 1] = '\0';
  }
  if (minutenteil->tag_max_ms > s_heute.minute_max_ms) {
    s_heute.minute_max_ms = minutenteil->tag_max_ms;
  }
  const uint32_t schlange = boulder_messung_schlange_min();
  if (schlange < s_heute.schlange_min) {
    s_heute.schlange_min = (uint8_t)schlange;
  }
}

static void prv_log(const BoulderDiagnoseTag *d) {
  PBL_LOG_INFO("Boulder Diagnose Tag %u: laengster Schritt %u ms (%s), Minutenteil %u ms, "
               "Heap min %lu B, Schlange min %u, Neustarts %u",
               d->tag, d->max_ms, d->max_name, d->minute_max_ms, (unsigned long)d->heap_min,
               d->schlange_min, d->neustarts);
}

void boulder_diagnose_takt(time_t jetzt) {
  if (!s_geladen) {
    return;
  }
  const uint16_t tag = time_util_get_day(jetzt);
  if (tag != s_heute.tag) {
    prv_auffrischen();
    prv_log(&s_heute);
    prv_neuer_tag(tag);
  }
  // NIEDRIGSTAND: Die Analytik setzt die Hochwassermarke des Kernel-Heaps stuendlich auf den
  // aktuellen Stand zurueck (kernel_heap.c:63). Je Minute gelesen, entgeht hoechstens eine
  // Spitze in der Minute vor diesem Zuruecksetzen.
  const uint32_t frei = heap_get_minimum_headroom(kernel_heap_get());
  if (frei < s_heute.heap_min) {
    s_heute.heap_min = frei;
  }
  prv_auffrischen();
}

void boulder_diagnose_laden(SettingsFile *datei, time_t jetzt) {
  const uint16_t tag = time_util_get_day(jetzt);
  BoulderDiagnoseTag d;
  const int n = (int)sizeof(d);
  if (datei && settings_file_get_len(datei, DIAGNOSE_SCHLUESSEL, 4) == n &&
      settings_file_get(datei, DIAGNOSE_SCHLUESSEL, 4, &d, sizeof(d)) == S_SUCCESS &&
      d.fassung == BOULDER_DIAGNOSE_FASSUNG) {
    s_datei = d;
    if (d.tag == tag) {
      // Dieselben Tageswerte weiter: der Hoechstwert vor dem Neustart ist der wichtigste.
      s_heute = d;
    } else {
      prv_log(&d);  // der letzte Tag vor dem Neustart, falls ihn kein Tageswechsel meldete
      prv_neuer_tag(tag);
    }
  } else {
    prv_neuer_tag(tag);
  }
  if (s_heute.neustarts < UINT16_MAX) {
    s_heute.neustarts++;
  }
  s_geladen = true;
}

void boulder_diagnose_ablegen(SettingsFile *datei) {
  if (!s_geladen || memcmp(&s_heute, &s_datei, sizeof(s_heute)) == 0) {
    return;
  }
  if (settings_file_set(datei, DIAGNOSE_SCHLUESSEL, 4, &s_heute, sizeof(s_heute)) ==
      S_SUCCESS) {
    s_datei = s_heute;
  } else {
    PBL_LOG_WRN("Boulder: Diagnose nicht geschrieben");
  }
}

const BoulderDiagnoseTag *boulder_diagnose_heute(void) {
  return &s_heute;
}

void boulder_diagnose_test_zuruecksetzen(void) {
  memset(&s_heute, 0, sizeof(s_heute));
  memset(&s_datei, 0, sizeof(s_datei));
  s_geladen = false;
}
