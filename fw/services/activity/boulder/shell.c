/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// Boulder V2: Shell-Befehle der Variante (Spezifikation 7.5, AP9).
//   boulder zeiten         Laufzeit je Arbeit, Heap, Schlange, Diagnosesatz des Tages
//   boulder prueflauf <n>  nur im Pruefbau (CONFIG_BOULDER_PRUEFLAUF, prueflauf.c)
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
// Shell-Befehle laufen auf KernelBG (shell.h): heap_calc_totals geht die Blockliste durch,
// das ist hier erlaubt, im Takt nicht.

#if defined(CONFIG_SHELL) && !defined(CONFIG_RECOVERY_FW)
#include <pbl/shell/shell.h>

#include "kernel/kernel_heap.h"
#include "pbl/services/activity/boulder/diagnose.h"
#include "pbl/services/activity/boulder/takt.h"

#include <inttypes.h>
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
  if (!boulder_prueflauf_starten(n)) {
    pbl_shell_print(sh, "Pruefnacht %d unbekannt oder ein Lauf laeuft (0-3 Fixtures, "
                        "4 schlimmste Nacht, 5 Velo)", n);
    return -1;
  }
  pbl_shell_print(sh, "Pruefnacht %d laeuft; Ende im Log, Zeiten mit 'boulder zeiten'", n);
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
  PBL_SHELL_CMD(pins, NULL, "Pins der Insights mit Open App", prv_pins),
  PBL_SHELL_CMD_ARG(stufe, NULL, "Health-App setzt die Messstufe ueber S4 <1-5|6>", prv_stufe,
                    2, 0),
  PBL_SHELL_CMD_ARG(ziele, NULL, "Ziele wie das Telefon <schritte> <min> [fassung] [laenge]",
                    prv_ziele, 2, 3),
  PBL_SHELL_CMD_ARG(spo2, NULL, "SpO2-Ring fuellen <n>", prv_spo2, 2, 0),
  PBL_SHELL_CMD(lage, NULL, "Lage der Health-App", prv_lage),
#endif
#endif
  PBL_SHELL_SUBCMD_SET_END,
};

PBL_SHELL_CMD_REGISTER(boulder, sub_boulder, "Boulder-Kraepelin", NULL);
#endif
