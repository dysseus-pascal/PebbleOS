/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// Boulder V2: Shell-Befehle der Variante (Spezifikation 7.5, AP9).
//   boulder zeiten         Laufzeit je Arbeit, Heap, Schlange, Diagnosesatz des Tages
//   boulder prueflauf <n> [tage]  nur im Pruefbau (CONFIG_BOULDER_PRUEFLAUF, prueflauf.c); `tage`
//                          Fenster vor dem juengsten (Schritt 3, AP3)
//   boulder wecker <h> <m> [einmal|gewoehnlich]  nur im Pruefbau: Smart Alarm anlegen, oder
//                          einen gewoehnlichen taeglichen Wecker (QEMU-Ablaeufe zum Neustart,
//                          wecklauf.h)
//   boulder stapel         nur im Pruefbau: Tiefstand der Syscall-Stapel seit dem Boot (Schritt 3,
//                          T2.5)
//   boulder probe          nur im Pruefbau mit der Health-App: oeffnet sie fuer die Probe der
//                          Bausteine ohne Systemrechte (Schritt 3, AP1; apps/.../probe.h)
//   boulder fehlerprobe    dasselbe fuer die Fehlerprobe: Die App schreibt in Kernel-RAM (T2.1)
//   boulder pins           nur im Pruefbau mit der Health-App: Pebbles Pins der Insights mit
//                          "Open App" anlegen (Schritt 3, AP7, T3.4)
//   boulder ziele <schritte> <min> [fassung] [laenge] | weg | leeren
//                          nur im Pruefbau mit der Health-App: "boulder_ziele" in die BlobDB Health
//                          schreiben, loeschen oder die Datenbank leeren - ueber blob_db_* wie das
//                          Telefon, mit Pebbles Ereignis (Schritt 3, AP2, T6.3)
//   boulder spo2 <n>       dasselbe: den SpO2-Ring mit n Werten fuellen (8.1)
//   boulder stufe <n>      dasselbe: Die Health-App setzt die Messstufe n ueber S4 (6: ungueltig)
//   boulder lage           dasselbe: die zuletzt veroeffentlichte Lage (S2 ohne die App)
//   boulder naechte        dasselbe: die Liste der Naechte im Kern, juengster Wachtag zuerst
//                          (Schritt 3, AP3; S5 ohne die App)
//   boulder herz           dasselbe: Herzband und Zonen im Kern (Schritt 3, AP4; S6 ohne die App)
//   boulder woche          dasselbe: die Woche und der Spiegel im Kern, dazu Pebbles Tageswerte
//                          (S7 ohne die App, T4.5)
//   boulder schritte <n>   dasselbe: Pebbles Schritte von heute auf n setzen (QEMU hat keine
//                          Bewegung; activity_metrics_set_metric_exact wie qemu_serial.c)
//   boulder zeit <utc>     dasselbe: die Uhr stellen wie das Telefon (clock_set_time), etwa kurz
//                          vor Mitternacht fuer den Tageswechsel der Woche
//   boulder bewegung <wochentag 0-6> <schritte> [utc]  dasselbe: einen Satz "<tag>_movementData"
//                          in die BlobDB Health, wie ihn das Telefon schickt (Gegenprobe T4.5)
// Shell-Befehle laufen auf KernelBG (shell.h): heap_calc_totals geht die Blockliste durch,
// das ist hier erlaubt, im Takt nicht.

#if defined(CONFIG_SHELL) && !defined(CONFIG_RECOVERY_FW)
#include <pbl/shell/shell.h>

#include "kernel/kernel_heap.h"
#include "pbl/services/activity/boulder/diagnose.h"
#include "pbl/services/activity/boulder/takt.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int prv_zeiten(const struct pbl_shell *sh, size_t argc, char **argv) {
  BoulderAufgabe *const *liste;
  const int n = boulder_messung_liste(&liste);
  pbl_shell_print(sh, "%-26s %8s %7s %9s", "Arbeit", "Laeufe", "max ms", "Mittel us");
  for (int i = 0; i < n; i++) {
    const BoulderAufgabe *a = liste[i];
    if (!a || !a->laeufe) {
      continue;
    }
    const uint32_t mittel_us =
        (uint32_t)(((uint64_t)a->summe_ticks * 1000000 / RTC_TICKS_HZ) / a->laeufe);
    pbl_shell_print(sh, "%-26s %8" PRIu32 " %7u %9" PRIu32, a->name, a->laeufe, a->max_ms,
                    mittel_us);
  }
  unsigned int belegt, frei, groesster;
  heap_calc_totals(kernel_heap_get(), &belegt, &frei, &groesster);
  pbl_shell_print(sh, "Heap: %u B gesamt, %u frei, groesster Block %u, Niedrigstand %" PRIu32,
                  (unsigned)heap_size(kernel_heap_get()), frei, groesster,
                  heap_get_minimum_headroom(kernel_heap_get()));
  const BoulderDiagnoseTag *d = boulder_diagnose_heute();
  pbl_shell_print(sh,
                  "Tag %u: laengster Schritt %u ms (%s), Minutenteil %u ms, Heap min %" PRIu32
                  ", Schlange min %u, Neustarts %u",
                  d->tag, d->max_ms, d->max_name, d->minute_max_ms, d->heap_min,
                  d->schlange_min, d->neustarts);
  return 0;
}

#if defined(CONFIG_BOULDER_PRUEFLAUF)
#include "pbl/services/activity/boulder/prueflauf.h"

static int prv_prueflauf(const struct pbl_shell *sh, size_t argc, char **argv) {
  const int n = (argc > 1) ? atoi(argv[1]) : -1;
  const int tage = (argc > 2) ? atoi(argv[2]) : 0;
  if (!boulder_prueflauf_starten(n, tage)) {
    pbl_shell_print(sh,
                    "Pruefnacht %d (%d Tage zurueck) unbekannt oder ein Lauf laeuft (0-3 "
                    "Fixtures, 4 schlimmste Nacht, 5 Velo; 0-30 Tage)",
                    n, tage);
    return -1;
  }
  pbl_shell_print(sh,
                  "Pruefnacht %d laeuft, %d Tage zurueck; Ende im Log, Zeiten mit "
                  "'boulder zeiten'",
                  n, tage);
  return 0;
}

#include "pbl/services/alarms/alarm.h"

// Legt den Smart Alarm ueber dieselbe Schnittstelle an wie die Wecker-App (alarm_create): Der
// QEMU-Ablauf braucht einen Wecker, ohne die App per Tasten zu bedienen.
static int prv_wecker(const struct pbl_shell *sh, size_t argc, char **argv) {
  const int stunde = atoi(argv[1]), minute = atoi(argv[2]);
  if (stunde < 0 || stunde > 23 || minute < 0 || minute > 59) {
    pbl_shell_print(sh, "Weckzeit %s:%s ungueltig", argv[1], argv[2]);
    return -1;
  }
  const bool gewoehnlich = (argc > 3) && !strcmp(argv[3], "gewoehnlich");
  const bool einmal = (argc > 3) && !gewoehnlich;
  const AlarmInfo info = {
    .hour = stunde, .minute = minute, .is_smart = !gewoehnlich, .vibrate_enabled = true,
    .kind = einmal ? ALARM_KIND_JUST_ONCE : ALARM_KIND_EVERYDAY,
  };
  const AlarmId id = alarm_create(&info);
  pbl_shell_print(sh, "%s %d auf %02d:%02d (%s)", gewoehnlich ? "Wecker" : "Smart Alarm", id,
                  stunde, minute, einmal ? "einmal" : "taeglich");
  return (id == ALARM_INVALID_ID) ? -1 : 0;
}

#include "syscall/syscall_internal.h"

// Wie viele Byte der Syscall-Stapel von App und Worker seit dem Boot nie benutzt wurden (2 KiB
// je Stapel, syscall_internal.c). Nur Syscalls aus Code ohne Systemrechte laufen dort; der
// Tiefstand ist also der tiefste Syscall einer solchen App seit dem Boot. Darum misst T2.5 nach
// frischem Boot.
static int prv_stapel(const struct pbl_shell *sh, size_t argc, char **argv) {
  pbl_shell_print(sh, "Syscall-Stapel nie benutzt: App %u B, Worker %u B",
                  (unsigned)syscall_app_stack_free_bytes(),
                  (unsigned)syscall_worker_stack_free_bytes());
  return 0;
}

#if defined(CONFIG_BOULDER_HEALTH_APP)
#include "apps/system/boulder_health/probe.h"
#include "pbl/services/activity/activity.h"
#include "process_management/app_manager.h"
#include "shell/system_app_ids.auto.h"

// Oeffnet die Health-App wie "Open App" aus einem Pin, mit der Probe als Startargument: Nur fuer
// APP_LAUNCH_TIMELINE_ACTION reicht Pebble ein Argument an die App durch (probe.h).
static int prv_health_probe(const struct pbl_shell *sh, uint32_t probe, const char *name) {
  app_manager_put_launch_app_event(&(AppLaunchEventConfig){
    .id = APP_ID_HEALTH_APP,
    .common.args = (void *)(uintptr_t)probe,
    .common.reason = APP_LAUNCH_TIMELINE_ACTION,
  });
  pbl_shell_print(sh, "Health-App mit %s", name);
  return 0;
}

static int prv_probe(const struct pbl_shell *sh, size_t argc, char **argv) {
  return prv_health_probe(sh, BH_PROBE_BAUSTEINE, "der Probe der Bausteine");
}

static int prv_fehlerprobe(const struct pbl_shell *sh, size_t argc, char **argv) {
  return prv_health_probe(sh, BH_PROBE_FEHLER, "der Fehlerprobe");
}

// Oeffnet die Health-App wie "Open App" aus einem Pin mit dem Kartentyp <n> (0 Activity, 1 Night):
// derselbe Weg wie ein echter Pin, ohne einen anzulegen. Fuer die Starts von L1 (AP8).
static int prv_start(const struct pbl_shell *sh, size_t argc, char **argv) {
  return prv_health_probe(sh, (uint32_t)atoi(argv[1]), "einem Pin");
}

// Schaltet Health Tracking ein wie Settings -> Health (settings/health.c): QEMU startet mit einem
// frischen Flash, auf dem es aus ist.
static int prv_tracking(const struct pbl_shell *sh, size_t argc, char **argv) {
  activity_prefs_tracking_set_enabled(true);
  activity_start_tracking(false /* test_mode */);
  pbl_shell_print(sh, "Health Tracking an");
  return 0;
}

// L6 (AP8): die App misst ihre Syscalls (probe.h, BH_PROBE_SYSCALLS).
static int prv_syscalls(const struct pbl_shell *sh, size_t argc, char **argv) {
  return prv_health_probe(sh, BH_PROBE_SYSCALLS, "der Zeit je Syscall");
}

#include "pbl/services/hrm/hrm_manager.h"
#include "process_state/app_state/app_state.h"

// AP8: Heap der laufenden App (Groesse, belegt, Hoechststand) und ob ein Abo den Puls im
// Sekundentakt haelt. Der Zeiger auf den Zustand der App ist global (app_state.c), also auch
// aus dem Pruefbefehl lesbar; nur gueltig, solange eine App laeuft.
static int prv_app(const struct pbl_shell *sh, size_t argc, char **argv) {
  const Heap *heap = app_state_get_heap();
  pbl_shell_print(sh, "App-Heap: %u B, belegt %u, Hoechststand %u",
                  (unsigned)heap_size(heap), (unsigned)heap->current_size,
                  (unsigned)heap->high_water_mark);
  pbl_shell_print(sh, "Puls im Sekundentakt: %d", (int)hrm_manager_has_continuous_green_subscriber());
  return 0;
}

// Die App ohne Systemrechte setzt die Messstufe ueber S4 (Schritt 3, AP2; T6.1 in QEMU).
static int prv_stufe(const struct pbl_shell *sh, size_t argc, char **argv) {
  const int stufe = atoi(argv[1]);
  if (stufe < 1 || stufe > 6) {
    pbl_shell_print(sh, "Stufe %s: 1-5 (6 als ungueltige)", argv[1]);
    return -1;
  }
  return prv_health_probe(sh, BH_PROBE_STUFE + (uint32_t)stufe, "S4");
}

#include "pbl/services/activity/activity_insights.h"

// Legt Pins der Insights an, ueber Pebbles eigene Pruefhilfe (wie Pebbles Activity-Demo,
// apps/demo/activity_demo/activity_demo.c:564): je vier Pins zu Aktivitaet (jetzt) und Schlaf
// (7 Uhr heute), jeder mit "Open App" und dem Kartentyp, den Pebbles activity_insights.c setzt.
// Ohne Bewegung und ohne Telefon entstuenden in QEMU keine; so laeuft der Weg vom Pin in die
// Health-App ganz durch Pebbles Code (T3.4). Die Meldungen dazu kommen nur, wenn die Insights an
// sind (in QEMU aus).
static int prv_pins(const struct pbl_shell *sh, size_t argc, char **argv) {
  activity_insights_test_push_summary_pins();
  pbl_shell_print(sh, "Pins der Insights angelegt");
  return 0;
}

#include "pbl/services/activity/boulder/schnittstelle.h"
#include "pbl/services/activity/boulder/spo2.h"
#include "pbl/services/activity/boulder/ziele.h"
#include "pbl/services/blob_db/api.h"
#include <pbl/drivers/rtc.h>

// Wie das Telefon (T6.3, 8.1): ueber blob_db_insert, blob_db_delete und blob_db_flush, die
// healthdb schreiben und PEBBLE_BLOBDB_EVENT melden. Der Endpunkt des Telefons ruft dieselben.
// Fassung und Laenge lassen sich verstellen, um ungueltige Saetze zu schicken.
static int prv_ziele(const struct pbl_shell *sh, size_t argc, char **argv) {
  const uint8_t *schluessel = (const uint8_t *)BOULDER_ZIELE_SCHLUESSEL;
  const int schluessel_len = strlen(BOULDER_ZIELE_SCHLUESSEL);
  status_t st;
  if (!strcmp(argv[1], "weg")) {
    st = blob_db_delete(BlobDBIdHealth, schluessel, schluessel_len);
    pbl_shell_print(sh, "boulder_ziele geloescht (%d)", (int)st);
  } else if (!strcmp(argv[1], "leeren")) {
    st = blob_db_flush(BlobDBIdHealth);
    pbl_shell_print(sh, "BlobDB Health geleert (%d)", (int)st);
  } else {
    if (argc < 3) {
      pbl_shell_print(sh, "boulder ziele <schritte> <min> [fassung] [laenge]");
      return -1;
    }
    const BoulderZieleSatz satz = {
      .fassung = (uint8_t)((argc > 3) ? atoi(argv[3]) : BOULDER_ZIELE_FASSUNG),
      .schritte = (uint16_t)atoi(argv[1]),
      .schlaf_min = (uint16_t)atoi(argv[2]),
    };
    uint8_t wert[12] = { 0 };
    memcpy(wert, &satz, sizeof(satz));
    const int laenge = (argc > 4) ? atoi(argv[4]) : (int)sizeof(satz);
    if (laenge < 1 || laenge > (int)sizeof(wert)) {
      pbl_shell_print(sh, "Laenge %d: 1 bis %d B", laenge, (int)sizeof(wert));
      return -1;
    }
    st = blob_db_insert(BlobDBIdHealth, schluessel, schluessel_len, wert, laenge);
    pbl_shell_print(sh, "boulder_ziele %u Schritte, %u min, Fassung %u, %d B geschrieben (%d)",
                    satz.schritte, satz.schlaf_min, satz.fassung, laenge, (int)st);
  }
  return (st == S_SUCCESS) ? 0 : -1;
}

// Den Ring wie aus dem Minutensatz fuellen: alle 10 min ein Wert bis jetzt, 94-98 %.
static int prv_spo2(const struct pbl_shell *sh, size_t argc, char **argv) {
  const int n = atoi(argv[1]);
  const time_t jetzt = rtc_get_time();
  for (int i = 0; i < n; i++) {
    boulder_spo2_merken(jetzt - (time_t)(n - i) * 10 * 60, (uint8_t)(94 + i % 5));
  }
  pbl_shell_print(sh, "SpO2: %d Werte gemerkt", n);
  return 0;
}

static int prv_lage(const struct pbl_shell *sh, size_t argc, char **argv) {
  BoulderLage l;
  boulder_lage_lesen(&l);
  pbl_shell_print(sh,
                  "Lage %u: stand %lu fenster %lu nacht %u schlaeft %u stufe %u spo2 %u min",
                  l.fassung, (unsigned long)l.stand_utc, (unsigned long)l.fenster_utc,
                  l.nacht_stufe, l.schlaeft, l.messstufe, l.spo2_takt_min);
  pbl_shell_print(sh, "Lage ziele %u %u maxpuls %u ruhepuls %u nickerchen %u", l.ziel_schritte,
                  l.ziel_schlaf_min, l.maxpuls, l.ruhepuls_median, l.nickerchen);
  return 0;
}

#include "pbl/services/activity/boulder/naechte.h"

// Ohne Kopie und ohne Sperre: Shell-Befehle laufen auf KernelBG, und nur KernelBG schreibt die
// Liste (naechte.h). Gedruckt wird ausserhalb jeder Sperre.
static int prv_naechte(const struct pbl_shell *sh, size_t argc, char **argv) {
  const BoulderNaechte *n = boulder_naechte_kern();
  pbl_shell_print(sh, "Naechte %d: anzahl %d neu %d min %d/%d", (int)n->fassung, (int)n->anzahl,
                  (int)n->neu, (int)n->min_ruhe, (int)n->min_hrv);
  for (int k = 0; k < n->anzahl; k++) {
    const BoulderNachtEintrag *e = &n->tage[(n->neu + NAECHTE_MAX - k) % NAECHTE_MAX];
    pbl_shell_print(sh, "Nacht %d: tag %d hrv %d ruhepuls %d", k, (int)e->tag, (int)e->hrv,
                    (int)e->ruhepuls);
  }
  return 0;
}

#include "pbl/services/activity/activity_private.h"
#include "pbl/services/activity/boulder/herz.h"
#include "pbl/services/activity/boulder/woche.h"
#include "pbl/services/clock.h"

// Wie "boulder naechte": ohne Kopie, auf KernelBG. Je Zeile vier Halbstunden mit Puls:
// Platz:min/max/mittel/n, aelteste zuerst.
static int prv_herz(const struct pbl_shell *sh, size_t argc, char **argv) {
  const BoulderHerz *h = boulder_herz_kern();
  pbl_shell_print(sh, "Herz bereit %d neueste %lu grenzen %d/%d/%d", boulder_herz_bereit(),
                  (unsigned long)h->neueste, h->grenzen[0], h->grenzen[1], h->grenzen[2]);
  pbl_shell_print(sh, "Herz zonen tag %d: %d %d %d %d", (int)h->zonen_tag, h->zonen_min[0],
                  h->zonen_min[1], h->zonen_min[2], h->zonen_min[3]);
  char zeile[96];
  int laenge = 0;
  int belegt = 0;
  for (int k = 0; k < HERZ_HALBSTUNDEN && h->neueste; k++) {
    const uint32_t halbstunde = h->neueste - (HERZ_HALBSTUNDEN - 1) + k;
    const int platz = (int)(halbstunde % HERZ_HALBSTUNDEN);
    if (!h->n[platz]) {
      continue;
    }
    belegt++;
    laenge += snprintf(zeile + laenge, sizeof(zeile) - laenge, " %d:%d/%d/%d/%d", platz,
                       h->min[platz], h->max[platz], h->mittel[platz], h->n[platz]);
    if (belegt % 4 == 0) {
      pbl_shell_print(sh, "Herz%s", zeile);
      laenge = 0;
    }
  }
  if (laenge) {
    pbl_shell_print(sh, "Herz%s", zeile);
  }
  pbl_shell_print(sh, "Herz halbstunden %d", belegt);
  return 0;
}

static int prv_woche(const struct pbl_shell *sh, size_t argc, char **argv) {
  const BoulderWoche *w = boulder_woche_kern();
  const BoulderSchrittSpiegel *sp = boulder_woche_spiegel();
  pbl_shell_print(sh, "Woche tag %d: %ld %ld %ld %ld %ld %ld; spiegel tag %d %lu", (int)w->tag,
                  (long)(int32_t)w->schritte[0], (long)(int32_t)w->schritte[1],
                  (long)(int32_t)w->schritte[2], (long)(int32_t)w->schritte[3],
                  (long)(int32_t)w->schritte[4], (long)(int32_t)w->schritte[5], (int)sp->tag,
                  (unsigned long)sp->schritte);
  // Pebbles Tageswerte zum Vergleich: heute und die Tage 1-6 (mit Verlauf: hier auf KernelBG
  // erlaubt, in der App nicht).
  int32_t pebble[ACTIVITY_HISTORY_DAYS];
  activity_get_metric(ActivityMetricStepCount, ACTIVITY_HISTORY_DAYS, pebble);
  pbl_shell_print(sh, "Pebble heute %ld, Tage 1-6: %ld %ld %ld %ld %ld %ld", (long)pebble[0],
                  (long)pebble[1], (long)pebble[2], (long)pebble[3], (long)pebble[4],
                  (long)pebble[5], (long)pebble[6]);
  return 0;
}

static int prv_schritte(const struct pbl_shell *sh, size_t argc, char **argv) {
  activity_metrics_set_metric_exact(ActivityMetricStepCount, atoi(argv[1]));
  pbl_shell_print(sh, "Pebbles Schritte heute: %s", argv[1]);
  return 0;
}

static int prv_zeit(const struct pbl_shell *sh, size_t argc, char **argv) {
  const time_t utc = (time_t)strtoul(argv[1], NULL, 10);
  clock_set_time(utc);
  pbl_shell_print(sh, "Uhr auf %lu", (unsigned long)rtc_get_time());
  return 0;
}

// Wie health_db.c ihn liest (MovementData, Fassung 1): sieben Worte.
static int prv_bewegung(const struct pbl_shell *sh, size_t argc, char **argv) {
  static const char *const s_tage[] = { "sunday", "monday", "tuesday", "wednesday",
                                        "thursday", "friday", "saturday" };
  const int wochentag = atoi(argv[1]);
  if (wochentag < 0 || wochentag > 6) {
    pbl_shell_print(sh, "Wochentag %s: 0 (Sonntag) bis 6", argv[1]);
    return -1;
  }
  const uint32_t satz[7] = {
    1, (argc > 3) ? (uint32_t)strtoul(argv[3], NULL, 10) : (uint32_t)rtc_get_time(),
    (uint32_t)strtoul(argv[2], NULL, 10), 0, 0, 0, 0,
  };
  char schluessel[32];
  snprintf(schluessel, sizeof(schluessel), "%s_movementData", s_tage[wochentag]);
  const status_t st = blob_db_insert(BlobDBIdHealth, (const uint8_t *)schluessel,
                                     strlen(schluessel), (const uint8_t *)satz, sizeof(satz));
  pbl_shell_print(sh, "%s %lu Schritte, Zeit %lu (%d)", schluessel, (unsigned long)satz[2],
                  (unsigned long)satz[1], (int)st);
  return (st == S_SUCCESS) ? 0 : -1;
}
#endif
#endif

static const struct pbl_shell_cmd sub_boulder[] = {
  PBL_SHELL_CMD(zeiten, NULL, "Laufzeiten, Heap, Diagnose", prv_zeiten),
#if defined(CONFIG_BOULDER_PRUEFLAUF)
  PBL_SHELL_CMD(prueflauf, NULL, "Pruefnacht im Zeitraffer", prv_prueflauf),
  PBL_SHELL_CMD_ARG(wecker, NULL, "Wecker anlegen <h> <m> [einmal|gewoehnlich]", prv_wecker, 3,
                    1),
  PBL_SHELL_CMD(stapel, NULL, "Tiefstand der Syscall-Stapel seit dem Boot", prv_stapel),
#if defined(CONFIG_BOULDER_HEALTH_APP)
  PBL_SHELL_CMD(probe, NULL, "Health-App: Probe der Bausteine", prv_probe),
  PBL_SHELL_CMD(fehlerprobe, NULL, "Health-App: Fehlerprobe in Kernel-RAM", prv_fehlerprobe),
  PBL_SHELL_CMD(syscalls, NULL, "Health-App: Zeit je Syscall (L6)", prv_syscalls),
  PBL_SHELL_CMD_ARG(start, NULL, "Health-App wie aus einem Pin <kartentyp>", prv_start, 2, 0),
  PBL_SHELL_CMD(tracking, NULL, "Health Tracking einschalten", prv_tracking),
  PBL_SHELL_CMD(app, NULL, "Heap der App, Puls im Sekundentakt", prv_app),
  PBL_SHELL_CMD(pins, NULL, "Pins der Insights mit Open App", prv_pins),
  PBL_SHELL_CMD_ARG(stufe, NULL, "Health-App setzt die Messstufe ueber S4 <1-5|6>", prv_stufe,
                    2, 0),
  PBL_SHELL_CMD_ARG(ziele, NULL, "Ziele wie das Telefon <schritte> <min> [fassung] [laenge]",
                    prv_ziele, 2, 3),
  PBL_SHELL_CMD_ARG(spo2, NULL, "SpO2-Ring fuellen <n>", prv_spo2, 2, 0),
  PBL_SHELL_CMD(lage, NULL, "Lage der Health-App", prv_lage),
  PBL_SHELL_CMD(naechte, NULL, "Naechte der Health-App (S5)", prv_naechte),
  PBL_SHELL_CMD(herz, NULL, "Herzband und Zonen der Health-App (S6)", prv_herz),
  PBL_SHELL_CMD(woche, NULL, "Woche der Health-App (S7) und Pebbles Tageswerte", prv_woche),
  PBL_SHELL_CMD_ARG(schritte, NULL, "Pebbles Schritte heute setzen <n>", prv_schritte, 2, 0),
  PBL_SHELL_CMD_ARG(zeit, NULL, "Uhr stellen <utc>", prv_zeit, 2, 0),
  PBL_SHELL_CMD_ARG(bewegung, NULL, "movementData wie das Telefon <tag> <schritte> [utc]",
                    prv_bewegung, 3, 1),
#endif
#endif
  PBL_SHELL_SUBCMD_SET_END,
};

PBL_SHELL_CMD_REGISTER(boulder, sub_boulder, "Boulder-Kraepelin", NULL);
#endif
