/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// Boulder V2: der QEMU-Pruefbau (prueflauf.h). Nur mit CONFIG_BOULDER_PRUEFLAUF gebaut.

#include "pbl/services/activity/boulder/prueflauf.h"

#include "kernel/kernel_heap.h"
#include "kernel/pbl_malloc.h"
#include "pbl/kernel/mutex.h"
#include <pbl/drivers/rtc.h>
#include "pbl/services/activity/activity.h"
#include "pbl/services/activity/activity_algorithm.h"
#include "pbl/services/activity/activity_private.h"
#include "pbl/services/activity/boulder/ausgabe.h"
#include "pbl/services/activity/boulder/kontext.h"
#include "pbl/services/activity/boulder/nacht.h"
#include "pbl/services/activity/boulder/puls.h"
#include "pbl/services/activity/boulder/schlafanalyse.h"
#include "pbl/services/activity/boulder/takt.h"
#include "pbl/services/new_timer/new_timer.h"
#include "pbl/services/system_task.h"
#include "pbl/util/units.h"
#include <pbl/logging/logging.h>

PBL_LOG_MODULE_DECLARE(service_activity, CONFIG_SERVICE_ACTIVITY_LOG_LEVEL);

#define NR_SCHLIMMSTE 4
#define NR_VELO 5
// Nach der Nacht wach und unterwegs bis 15 Uhr (Fensterminute 1140): Die Nacht wird
// "endgueltig", geht ans Datalogging und schliesst ihre Zyklen - wie test_boulder_zyklen.
#define BIS_NACHT (19 * 60)
#define UNTERWEGS_VMC 3000
#define UNTERWEGS_PULS 82

typedef struct {
  int nr;
  int tage;        //< so viele Fenster vor dem juengsten (Schritt 3, AP3)
  int m;           //< naechste Minute
  int bis;
  time_t beginn;   //< Minute 0
  int hrv;         //< naechstes HRV-Fenster
  RtcTicks start;
} Lauf;

static Lauf s_lauf;
static bool s_laeuft;
static void *s_ballast;
static bool s_vorbereitet;
static TimerID s_wieder;

// Nur gemessen: der Takt, den der Pruefbau je Minute selbst ruft (sonst der Cron).
static BoulderAufgabe s_messung_takt = BOULDER_AUFGABE("Boulder Takt (Pruefbau)", NULL);

// ---------------------------------------------------------------------------------------
// Die schlimmste Nacht: im Bett von Minute 60 bis 900, zehn Zyklen zu 84 Minuten (leicht,
// tief, leicht, ruhig, kurz wach), Puls jede Minute, ein HRV-Fenster alle 10 Minuten.

static void prv_schlimmste(int m, uint16_t *vmc, uint8_t *puls) {
  const uint32_t zufall = (uint32_t)m * 2654435761u;
  if (m < 60 || m >= 900) {
    *vmc = 2400 + zufall % 900;
    *puls = 80 + zufall % 12;
    return;
  }
  const int p = (m - 60) % 84;
  if (p < 10) {
    *vmc = 40 + zufall % 80;
  } else if (p < 38) {
    *vmc = zufall % 6;
  } else if (p < 62) {
    *vmc = 20 + zufall % 70;
  } else if (p < 79) {
    *vmc = 5 + zufall % 10;
  } else {
    *vmc = 600 + zufall % 600;
  }
  *puls = 48 + (p / 4) + zufall % 6;
}

// Die Velofahrt (wie test_boulder_fahrt.c): 3 min gehen, 25 min Velo, 6 min gehen.
static void prv_velo(int m, BoulderMinute *aus) {
  if (m < 3 || m >= 28) {
    aus->schritte = (uint8_t)(95 + m % 7);
    aus->vmc = (uint16_t)(2600 + (m % 5) * 90);
    aus->lage = (uint8_t)(0x41 + m % 4);
    aus->puls = 92;
    return;
  }
  aus->schritte = (uint8_t)(3 + (m * 7) % 16);
  aus->vmc = (uint16_t)(700 + (m * 37) % 600);
  aus->lage = (m % 5 == 4) ? 0x38 : 0x37;
  aus->puls = 104;
}

// ---------------------------------------------------------------------------------------

static void prv_glied(void *data);

static uint32_t s_frei_leer;  //< freie Plaetze der KernelBG-Schlange, wenn sie leer ist

// WIE AUF DER UHR: Die naechste Minute erst, wenn alles, was die letzte eingereiht hat, gelaufen
// ist (Nachlauf, Ausgabe in Teilen, Anlage, Ablage). Auf der Uhr liegen Minuten 60 s
// auseinander; stuende hier zwischen zwei Teilen der Ausgabe schon die naechste Minute, finge
// jeder Nachlauf die Sendung von vorn an. Geprueft wird ueber den NewTimers-Zeitgeber, damit
// KernelBG frei bleibt.
// Leer heisst: kein Callback laeuft (der Zeitgeber kann mitten in einen hineinfallen, der
// gleich den naechsten Teil einreiht) und so viel frei wie je gesehen.
// DIE FRAGE ABWARTEN: Im Zeitraffer liefe die Antwortfrist (Minuten der Fahrt) in
// Millisekunden ab. Steht eine Frage, haelt der Lauf an, bis sie beantwortet ist, hoechstens
// zwei Minuten echte Zeit - Zeit fuer Bildschirmfoto und Knopfdruck im Emulator.
#define FRAGE_WARTEN_MS (2 * 60 * 1000)
static RtcTicks s_frage_seit;

static bool prv_frage_wartet(void) {
  if (s_lauf.nr != NR_VELO || !boulder_kontext_frage_offen()) {
    s_frage_seit = 0;
    return false;
  }
  if (!s_frage_seit) {
    s_frage_seit = rtc_get_ticks();
  }
  return (rtc_get_ticks() - s_frage_seit) * 1000 / RTC_TICKS_HZ < FRAGE_WARTEN_MS;
}

static void prv_wieder_cb(void *data) {
  if (prv_frage_wartet()) {
    new_timer_start(s_wieder, 100, prv_wieder_cb, NULL, 0);
    return;
  }
  const uint32_t frei = system_task_get_available_space();
  if (frei > s_frei_leer) {
    s_frei_leer = frei;
  }
  if (system_task_get_current_callback() || frei < s_frei_leer ||
      !system_task_add_callback(prv_glied, NULL)) {
    new_timer_start(s_wieder, 2, prv_wieder_cb, NULL, 0);
  }
}

static void prv_weiter(void) {
  new_timer_start(s_wieder, 1, prv_wieder_cb, NULL, 0);
}

static void prv_ende(void) {
  s_laeuft = false;
  boulder_dauerteil_prueflauf(false);
  unsigned int belegt, frei, groesster;
  heap_calc_totals(kernel_heap_get(), &belegt, &frei, &groesster);
  const BoulderGesendet *g = boulder_ausgabe_gesendet();
  PBL_LOG_INFO("Boulder Pruefbau %d fertig: %d Minuten in %lu ms; geschickt Nacht %lu, "
               "Schlaf %u min; Heap frei %u, groesster Block %u, Niedrigstand %lu",
               s_lauf.nr, s_lauf.bis,
               (unsigned long)((rtc_get_ticks() - s_lauf.start) * 1000 / RTC_TICKS_HZ),
               (unsigned long)g->beginn, g->schlaf, frei, groesster,
               (unsigned long)heap_get_minimum_headroom(kernel_heap_get()));
}

static void prv_glied(void *data) {
  if (s_lauf.m >= s_lauf.bis) {
    prv_ende();
    return;
  }
  const int m = s_lauf.m++;
  const time_t utc = s_lauf.beginn + (time_t)m * PBL_SEC_PER_MIN;
  BoulderMinute minute = {.utc = utc};
  if (s_lauf.nr == NR_VELO) {
    prv_velo(m, &minute);
  } else if (m >= SCHLAF_MINUTEN_MAX) {
    minute.vmc = UNTERWEGS_VMC;
    minute.puls = UNTERWEGS_PULS;
  } else if (s_lauf.nr == NR_SCHLIMMSTE) {
    prv_schlimmste(m, &minute.vmc, &minute.puls);
    // Die HRV-Fenster wie die Messung sie liefert: fertig 3 Minuten nach ihrer Mitte.
    if (m >= 63 && m < 900 && (m - 63) % 10 == 0) {
      boulder_dauerteil_hrv_fenster(utc - 3 * PBL_SEC_PER_MIN, 30 + (m % 50), minute.puls);
    }
  } else {
    const PrueflaufNacht *n = &g_boulder_prueflauf_naechte[s_lauf.nr];
    // Wie test_boulder_zyklen: ein Fenster, sobald seine Mitte vor dem Nachlauf dieser
    // Minute liegt.
    while (s_lauf.hrv < n->hrv_anzahl && n->hrv[s_lauf.hrv].minute + 2 < m + 1 &&
           m < NACHT_MINUTEN) {
      const PrueflaufHrv *h = &n->hrv[s_lauf.hrv++];
      boulder_dauerteil_hrv_fenster(s_lauf.beginn + h->minute * PBL_SEC_PER_MIN, h->rmssd,
                                    h->puls);
    }
    const bool gueltig = n->vmc[m] != 0xFFFF;
    minute.vmc = gueltig ? n->vmc[m] : 0;
    minute.puls = gueltig ? n->puls[m] : 0;
    minute.nicht_getragen = !gueltig;
  }
  // DIE FIXTURES HABEN KEINE SCHRITTE UND KEINE LAGE (nur VMC und Puls). Wache, bewegte
  // Minuten ohne Schritte saehe der Kontext als Velofahrt - eine Scheinfahrt mit Dialog. Der
  // Pruefbau setzt dort Gehen ein (Kadenz aus der VMC, wechselnde Lage).
  if (s_lauf.nr != NR_VELO && minute.vmc >= SCHLAF_SCHWELLE_VMC) {
    minute.schritte = (uint8_t)((minute.vmc / 25 > 120) ? 120 : minute.vmc / 25);
    minute.lage = (uint8_t)(0x41 + m % 4);
  }
  boulder_dauerteil_prueflauf_minute(&minute);
  {
    BOULDER_AUFGABE_LAEUFT(&s_messung_takt);
    boulder_dauerteil_takt(utc + PBL_SEC_PER_MIN + 10);
  }
  prv_weiter();
}

bool boulder_prueflauf_ballast(void) {
  static bool s_versucht;
  if (s_versucht) {
    return s_ballast != NULL;
  }
  s_versucht = true;
  const size_t gesamt = heap_size(kernel_heap_get());
  const size_t ziel = CONFIG_BOULDER_PRUEFLAUF_HEAP;
  if (gesamt > ziel) {
    s_ballast = kernel_malloc(gesamt - ziel);
  }
  PBL_LOG_INFO("Boulder Pruefbau: Heap %u B, Ballast %u B (%s), Ziel %u B", (unsigned)gesamt,
               (unsigned)(gesamt - ziel), s_ballast ? "geholt" : "FEHLT", (unsigned)ziel);
  return s_ballast != NULL;
}

// Vorbereitung einmal je Boot, in einem eigenen Callback (die Datei zu fuellen dauert).
static void prv_vorbereiten(void) {
  if (s_vorbereitet) {
    return;
  }
  s_vorbereitet = true;
  if (!s_wieder) {
    s_wieder = new_timer_create();
  }
  boulder_prueflauf_ballast();
  if (!activity_algorithm_test_fill_minute_file()) {
    PBL_LOG_WRN("Boulder Pruefbau: Minutendatei nicht gefuellt");
  }
}

static void prv_start_cb(void *data);

static void prv_start_wieder_cb(void *data) {
  system_task_add_callback(prv_start_cb, NULL);
}

static void prv_start_cb(void *data) {
  // Ein frisches QEMU-Abbild hat Health aus: einschalten wie Einstellungen > Health und
  // warten, bis der Tracking-Teil steht (der Pruefbau braucht ihn fuer die Minutendatei).
  if (!activity_tracking_on()) {
    if (!s_wieder) {
      s_wieder = new_timer_create();
    }
    activity_prefs_tracking_set_enabled(true);
    activity_start_tracking(false);
    new_timer_start(s_wieder, 3000, prv_start_wieder_cb, NULL, 0);
    return;
  }
  prv_vorbereiten();
  const time_t jetzt = rtc_get_time();
  time_t fenster = boulder_dauerteil_fenster(jetzt);
  s_lauf.start = rtc_get_ticks();
  if (s_lauf.nr == NR_VELO) {
    // Die Fahrt endet zwei Minuten vor jetzt, nach der Nacht des Fensters.
    s_lauf.bis = 34;
    s_lauf.beginn = jetzt - (jetzt % PBL_SEC_PER_MIN) - (s_lauf.bis + 2) * PBL_SEC_PER_MIN;
  } else {
    // Das juengste Fenster, dessen Lauf ganz vor jetzt liegt.
    s_lauf.bis = BIS_NACHT;
    if (fenster + (time_t)BIS_NACHT * PBL_SEC_PER_MIN > jetzt) {
      fenster = boulder_dauerteil_fenster(fenster - PBL_SEC_PER_MIN);
    }
    // Fruehere Fenster ueber eine Zeit mitten in ihnen: so stimmt auch ein Tag mit Zeitumstellung.
    if (s_lauf.tage > 0) {
      fenster = boulder_dauerteil_fenster(fenster - (time_t)s_lauf.tage * PBL_SEC_PER_DAY +
                                          3 * PBL_SEC_PER_HOUR);
    }
    s_lauf.beginn = fenster;
    // Jede Pruefnacht liegt im selben Fenster: Pebbles Liste (32 Plaetze) haelt sonst die
    // Sitzungen der vorigen Laeufe und laeuft ueber - auf der Uhr raeumt der Tageswechsel
    // sie weg. Hier raeumt der Pruefbau wie zwei Tage spaeter.
    ActivityState *zustand = activity_private_state();
    pbl_mutex_lock(&zustand->mutex, PBL_FOREVER);
    activity_sessions_prv_remove_out_of_range_activity_sessions(jetzt + 2 * PBL_SEC_PER_DAY,
                                                                true /*remove_ongoing*/);
    pbl_mutex_unlock(&zustand->mutex);
    boulder_dauerteil_test_neustart();
  }
  boulder_dauerteil_prueflauf(true);
  // Dieser Callback ist schon aus der Schlange: so viel ist frei, wenn sie leer ist.
  s_frei_leer = system_task_get_available_space();
  PBL_LOG_INFO("Boulder Pruefbau %d: %d Minuten ab %lu", s_lauf.nr, s_lauf.bis,
               (unsigned long)s_lauf.beginn);
  prv_glied(NULL);
}

bool boulder_prueflauf_starten(int nr, int tage) {
  if (s_laeuft || nr < 0 || nr > NR_VELO || tage < 0 || tage > 30) {
    return false;
  }
  s_laeuft = true;
  s_lauf = (Lauf){.nr = nr, .tage = tage};
  if (!system_task_add_callback(prv_start_cb, NULL)) {
    s_laeuft = false;
    return false;
  }
  return true;
}
