/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// Boulder V2: Shell-Befehle der Variante (Spezifikation 7.5, AP9).
//   boulder zeiten         Laufzeit je Arbeit, Heap, Schlange, Diagnosesatz des Tages
//   boulder prueflauf <n>  nur im Pruefbau (CONFIG_BOULDER_PRUEFLAUF, prueflauf.c)
//   boulder wecker <h> <m> [einmal]  nur im Pruefbau: Smart Alarm anlegen (QEMU-Ablauf
//                          "Neustart im Fenster", wecklauf.h)
// Shell-Befehle laufen auf KernelBG (shell.h): heap_calc_totals geht die Blockliste durch,
// das ist hier erlaubt, im Takt nicht.

#if defined(CONFIG_SHELL) && !defined(CONFIG_RECOVERY_FW)
#include <pbl/shell/shell.h>

#include "kernel/kernel_heap.h"
#include "pbl/services/activity/boulder/diagnose.h"
#include "pbl/services/activity/boulder/takt.h"

#include <inttypes.h>
#include <stdlib.h>

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
  const AlarmInfo info = {
    .hour = stunde, .minute = minute, .is_smart = true, .vibrate_enabled = true,
    .kind = (argc > 3) ? ALARM_KIND_JUST_ONCE : ALARM_KIND_EVERYDAY,
  };
  const AlarmId id = alarm_create(&info);
  pbl_shell_print(sh, "Smart Alarm %d auf %02d:%02d (%s)", id, stunde, minute,
                  (argc > 3) ? "einmal" : "taeglich");
  return (id == ALARM_INVALID_ID) ? -1 : 0;
}
#endif

static const struct pbl_shell_cmd sub_boulder[] = {
  PBL_SHELL_CMD(zeiten, NULL, "Laufzeiten, Heap, Diagnose", prv_zeiten),
#if defined(CONFIG_BOULDER_PRUEFLAUF)
  PBL_SHELL_CMD(prueflauf, NULL, "Pruefnacht im Zeitraffer", prv_prueflauf),
  PBL_SHELL_CMD_ARG(wecker, NULL, "Smart Alarm anlegen <h> <m> [einmal]", prv_wecker, 3, 1),
#endif
  PBL_SHELL_SUBCMD_SET_END,
};

PBL_SHELL_CMD_REGISTER(boulder, sub_boulder, "Boulder-Kraepelin", NULL);
#endif
