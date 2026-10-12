/* Probe der Gegenpruefung (nicht im Repo): "Laden" bei knapper KernelBG-Schlange im ersten init. */
#include "schnittstelle_umfeld.h"

void test_pk_laden__initialize(void) {
  prv_umfeld_beginn();
}

void test_pk_laden__cleanup(void) {
  prv_umfeld_ende();
}

void test_pk_laden__knappe_schlange_beim_ersten_init(void) {
  // Das Umfeld hat gebootet; KernelBG hat noch nichts laufen lassen. Neustart, diesmal mit 23
  // fremden Eintraegen in der Schlange des Kerns (7 frei < BOULDER_SCHLANGE_FREI = 8).
  prv_kernel_bg();
  activity_algorithm_deinit();
  prv_kernel_bg();
  s_abgestuerzt = false;
  s_abo_anzahl = 0;
  s_kern_n = 0;
  s_app_n = 0;
  boulder_dauerteil_test_neustart();
  boulder_messstufe_test_zuruecksetzen();
  boulder_naechte_test_neustart();
  boulder_herz_test_neustart();
  boulder_woche_test_neustart();
  for (int i = 0; i < 23; i++) {
    s_kern_schlange[s_kern_n++] = (Eintrag){ prv_fremd_cb, NULL };
  }
  const uint32_t laden_vorher = prv_laeufe("Boulder Laden");
  cl_assert(activity_algorithm_init(&s_rate));
  prv_kernel_bg();
  // Eine Stunde Minuten mit Puls, OHNE den Takt: Den plant auf der Uhr erst "Laden"
  // (dauerteil.c:178-180), ohne "Laden" gibt es keinen Cron-Takt.
  const time_t start = s_jetzt - (s_jetzt % PBL_SEC_PER_MIN);
  for (int m = 0; m < 60; m++) {
    const time_t utc = start + (time_t)m * PBL_SEC_PER_MIN;
    const BoulderMinute minute = { .utc = utc, .vmc = 1000, .puls = 70 };
    rtc_set_time(utc + PBL_SEC_PER_MIN);
    boulder_dauerteil_minute(&minute);
    activity_sessions_prv_minute_handler(utc + PBL_SEC_PER_MIN);
    prv_kernel_bg();
  }
  prv_app_beginn();
  BoulderNaechte *n = (BoulderNaechte *)&s_app_ram[3072];
  const bool s5 = sys_boulder_naechte(n);
  prv_app_ende();
  const bool s2 = prv_s2();
  const bool s6 = prv_s6();
  const bool s7 = prv_s7();
  printf("\nPROBE knappe Schlange beim ersten init: Laden %lu -> %lu, wartet %d, S2 %d S5 %d S6 %d "
         "S7 %d nach 60 Minuten",
         (unsigned long)laden_vorher, (unsigned long)prv_laeufe("Boulder Laden"),
         boulder_nacht_wartet(), s2, s5, s6, s7);
}
