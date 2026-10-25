/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// Boulder V2, Schritt 3: die Schnittstelle zur Health-App (schnittstelle.h; Spezifikation
// Schritt 3, 3.1-3.4; AP2: S1-S4, AP3: S5, AP4: S6 und S7). Nur mit der eigenen Health-App; ohne
// den Schalter bleibt die Datei leer.
#ifdef CONFIG_BOULDER_HEALTH_APP

#include "pbl/services/activity/boulder/schnittstelle.h"

#include "kernel/pebble_tasks.h"
#include "pbl/kernel/mutex.h"
#include "pbl/services/activity/activity.h"
#include "pbl/services/activity/boulder/ausgabe.h"
#include "pbl/services/activity/boulder/herz.h"
#include "pbl/services/activity/boulder/live.h"
#include "pbl/services/activity/boulder/messstufe.h"
#include "pbl/services/activity/boulder/naechte.h"
#include "pbl/services/activity/boulder/nacht.h"
#include "pbl/services/activity/boulder/takt.h"
#include "pbl/services/activity/boulder/woche.h"
#include "pbl/services/activity/boulder/ziele.h"
#ifdef CONFIG_BOULDER_KIESELSPORT
#include "pbl/services/activity/boulder/sport/kieselsport.h"
#endif
#include "pbl/services/system_task.h"
#include "pbl/util/math.h"
#include "pbl/util/testing.h"
#include <pbl/drivers/rtc.h>
#include <pbl/kernel/syscall.h>
#include <pbl/logging/logging.h>

#include <string.h>

PBL_LOG_MODULE_DECLARE(service_activity, CONFIG_SERVICE_ACTIVITY_LOG_LEVEL);

// ---------------------------------------------------------------------------------------------
// Die Lage

// EINE BLATTSPERRE (3.4): Unter ihr wird nur kopiert, nichts gerufen, keine zweite Sperre
// genommen. Fuer die Tests sichtbar (Sperrfolge, T2.4).
PBL_T_STATIC PBL_MUTEX_DEFINE(s_lage_sperre);

#define LAGE_VOR_DEM_ERSTEN_MAL                                                \
  ((BoulderLage){ .fassung = BOULDER_LAGE_FASSUNG,                             \
                  .ziel_schritte = ZIEL_SCHRITTE_VORGABE,                      \
                  .ziel_schlaf_min = ZIEL_SCHLAF_VORGABE })

static BoulderLage s_lage = LAGE_VOR_DEM_ERSTEN_MAL;

// "Heute gemeldete Nickerchen": Die Nacht zaehlt seit dem Boot (nacht.c). Gezaehlt wird ab dem
// Stand beim Beginn des laufenden Fensters (20 Uhr) - die Nickerchen liegen in dessen Tagteil.
// Nur KernelBG.
static time_t s_nickerchen_fenster;
static uint16_t s_nickerchen_basis;

static uint16_t prv_maxpuls(void) {
#ifdef CONFIG_BOULDER_KIESELSPORT
  // DER WERT, MIT DEM KIESELSPORT RECHNET (Einstellung nach Boulders Pruefung, sonst Tanaka). Ohne
  // Datei zu haben, sobald Kieselsport seine Einstellungen geladen hat ("Laden" startet es, vor der
  // ersten Lage) - AP2 hat das geprueft (3.4).
  return kieselsport_maxpuls_wirksam();
#else
  return 0;
#endif
}

// Misst die Uhr SpO2 nach Pebbles Plan, und wie oft? Die Stufe gibt den Takt. Gemessen wird mit
// "SpO2" den ganzen Tag, mit dem Schalter "Schlaf" in der Nacht (spo2_plan.h); ohne beide
// gar nicht, und die Health-App sagt "off". Die Schalter stellt der Nutzer (messstufe.h). Vor dem
// ersten Lesen der Stufe gilt die Vorgabe, wie im Regler.
static uint8_t prv_spo2_takt(uint8_t stufe) {
  if (!activity_prefs_spo2_is_enabled() && !boulder_spo2_schlaf_geladen()) {
    return 0;
  }
  return boulder_messstufe_werte(stufe)->spo2_min;
}

void boulder_lage_veroeffentlichen(void) {
  // ALLES OHNE SPERRE ZUSAMMENTRAGEN: Jede Quelle lebt auf KernelBG ohne eigene Sperre oder
  // nimmt ihre eigene Blattsperre (Kieselsport). Unter s_lage_sperre wird nur kopiert.
  const uint8_t stufe = boulder_messstufe_geladen();
  const time_t fenster = boulder_nacht_fenster();
  const uint16_t gemeldet = boulder_nacht_nickerchen_gemeldet();
  if (fenster != s_nickerchen_fenster || gemeldet < s_nickerchen_basis) {
    s_nickerchen_fenster = fenster;
    s_nickerchen_basis = gemeldet;
  }
  const BoulderLage neu = {
    .fassung = BOULDER_LAGE_FASSUNG,
    .messstufe = stufe,
    .nacht_stufe = (uint8_t)boulder_nacht_stufe(),
    .stand_utc = (uint32_t)rtc_get_time(),
    .fenster_utc = (uint32_t)fenster,
    .ziel_schritte = boulder_ziel_schritte(),
    .ziel_schlaf_min = boulder_ziel_schlaf_min(),
    .maxpuls = prv_maxpuls(),
    .ruhepuls_median = boulder_nacht_ruhepuls_median(),
    .schlaeft = boulder_live_schlaeft(),
    .spo2_takt_min = prv_spo2_takt(stufe),
    .nickerchen = (uint8_t)MIN(gemeldet - s_nickerchen_basis, UINT8_MAX),
  };
  pbl_mutex_lock(&s_lage_sperre, PBL_FOREVER);
  s_lage = neu;
  pbl_mutex_unlock(&s_lage_sperre);
}

void boulder_lage_lesen(BoulderLage *aus) {
  pbl_mutex_lock(&s_lage_sperre, PBL_FOREVER);
  *aus = s_lage;
  pbl_mutex_unlock(&s_lage_sperre);
}

// ---------------------------------------------------------------------------------------------
// Auftraege aus der App (3.1, Regel 6)

static void prv_bild_cb(void *data);
static void prv_stufe_cb(void *data);
static BoulderAufgabe s_auftrag_bild = BOULDER_AUFGABE("Boulder Bild laden", prv_bild_cb);
static BoulderAufgabe s_auftrag_stufe = BOULDER_AUFGABE("Boulder Messstufe setzen", prv_stufe_cb);

// Der Wunsch der App zur Stufe. Ein zweiter vor dem Setzen ersetzt den ersten; gesetzt wird der,
// der beim Lauf des Auftrags gilt.
static volatile uint8_t s_wunsch_stufe;

// DIE SCHLANGE DER APP, NICHT DIE VON KERNELBG: Aus der App-Task landet system_task_add_callback
// in deren eigener Schlange (8 Plaetze) und setzt die Uhr nie zurueck; ist sie voll, wartet die
// App. Ein Merker je Auftrag: Er steht hoechstens einmal an, also stellt die App hoechstens zwei
// Boulder-Auftraege ein (T6.2). Erst der Merker, dann das Einreihen - sonst koennte KernelBG den
// Auftrag laufen lassen, bevor er gesetzt ist, und er bliebe fuer immer stehen (takt.c).
static void prv_aus_der_app(BoulderAufgabe *auftrag) {
  if (pebble_task_get_current() != PebbleTask_App) {
    // Kein App-Aufrufer (Kern mit Systemrechten): dann der verwerfbare Weg des Kerns.
    boulder_aufgabe_einreihen(auftrag);
    return;
  }
  if (auftrag->steht_an) {
    return;
  }
  auftrag->steht_an = true;
  if (!system_task_add_callback(auftrag->lauf, NULL)) {
    auftrag->steht_an = false;
    PBL_LOG_WRN("Boulder: %s nicht eingereiht", auftrag->name);
  }
}

static void prv_bild_cb(void *data) {
  BOULDER_AUFGABE_LAEUFT(&s_auftrag_bild);
  boulder_nacht_bild_laden();
}

static void prv_stufe_cb(void *data) {
  BOULDER_AUFGABE_LAEUFT(&s_auftrag_stufe);
  // Wie das Menue in Settings -> Health: derselbe Schluessel, das Telefon erfaehrt es, Pebbles
  // Takte folgen im Auftrag der Messstufe. Die Datei schreibt es hier, auf KernelBG - auf dem
  // Syscall-Stapel der App waere dafuer kein Platz.
  boulder_messstufe_setzen(s_wunsch_stufe);
  boulder_lage_veroeffentlichen();
}

// ---------------------------------------------------------------------------------------------
// Die Syscalls. Ein Puffer der App muss ganz in ihrem Speicher liegen, sonst endet sie; das prueft
// PBL_SYSCALL nach der Art des Arguments, bevor der Rumpf laeuft. Mit Systemrechten (Kern) prueft
// es nichts.

PBL_SYSCALL(BoulderLesen, sys_boulder_nacht, (OUT_BUF, NachtBild *, aus, sizeof(NachtBild))) {
  const BoulderLesen ergebnis = boulder_nacht_bild_kopieren(aus);
  if (ergebnis == BoulderLesenSpaeter) {
    // NACH DEM FREIGEBEN DER SPERRE (3.2): Das Einreihen kann warten, wenn die Schlange der App
    // voll ist.
    prv_aus_der_app(&s_auftrag_bild);
  }
  return ergebnis;
}

PBL_SYSCALL(bool, sys_boulder_lage, (OUT_BUF, BoulderLage *, aus, sizeof(BoulderLage))) {
  pbl_mutex_lock(&s_lage_sperre, PBL_FOREVER);
  // MEMCPY STATT ZUWEISUNG: Fuer *aus = s_lage nimmt der Compiler LDM/STM, die eine Wortgrenze
  // verlangen; die Pruefung des Puffers prueft keine. Ein schiefer Puffer der App endete sonst als
  // UsageFault mit Systemrechten im Neustart. Ebenso S3 (spo2.c) und S7 (woche.c).
  memcpy(aus, &s_lage, sizeof(*aus));
  const bool veroeffentlicht = s_lage.stand_utc != 0;
  pbl_mutex_unlock(&s_lage_sperre);
  // PEBBLES SCHALTER FRISCH, ausserhalb der Sperre: Mit Tracking aus laeuft kein Minutenteil, der
  // die Lage veroeffentlichen koennte - "Tracking off" stuende sonst nie da.
  aus->tracking = activity_prefs_tracking_is_enabled();
  return veroeffentlicht;
}

// ERST BEGRENZEN, DANN PRUEFEN, darum im Rumpf: geprueft wird genau, was kopiert werden kann,
// hoechstens SPO2_WERTE_MAX Werte, und ohne Wunsch nichts.
PBL_SYSCALL(uint32_t, sys_boulder_spo2, (VAL, time_t, von), (VAL, time_t, bis),
            (VAL, Spo2Wert *, aus), (VAL, uint32_t, max)) {
  const uint32_t n = MIN(max, (uint32_t)SPO2_WERTE_MAX);
  if (n == 0) {
    return 0;
  }
  if (from_user) {
    pbl_syscall_verify_user(aus, n * sizeof(*aus));
  }
  // Der Ring kopiert unter seiner Sperre direkt in den Puffer (spo2.c).
  return (uint32_t)boulder_spo2_werte(von, bis, aus, (int)n);
}

PBL_SYSCALL(bool, sys_boulder_naechte, (OUT_BUF, BoulderNaechte *, aus, sizeof(BoulderNaechte))) {
  // Unter s_naechte_sperre nur die Kopie in den Puffer der App (naechte.c).
  return boulder_naechte_kopieren(aus);
}

PBL_SYSCALL(bool, sys_boulder_herz, (OUT_BUF, BoulderHerz *, aus, sizeof(BoulderHerz))) {
  // Unter s_herz_sperre nur die Kopie in den Puffer der App (herz.c).
  return boulder_herz_kopieren(aus);
}

PBL_SYSCALL(bool, sys_boulder_woche, (OUT_BUF, BoulderWoche *, aus, sizeof(BoulderWoche))) {
  // Unter s_woche_sperre nur die Kopie in den Puffer der App (woche.c).
  return boulder_woche_kopieren(aus);
}

PBL_SYSCALL(bool, sys_boulder_messstufe_setzen, (VAL, uint8_t, stufe)) {
  if (stufe < MESSSTUFE_MIN || stufe > MESSSTUFE_MAX) {
    return false;
  }
  // Erst der Wunsch, dann der Auftrag: Steht er schon an, setzt sein Lauf diesen Wunsch.
  s_wunsch_stufe = stufe;
  prv_aus_der_app(&s_auftrag_stufe);
  return true;
}

void boulder_schnittstelle_test_neustart(void) {
  s_lage = LAGE_VOR_DEM_ERSTEN_MAL;
  s_nickerchen_fenster = 0;
  s_nickerchen_basis = 0;
  s_wunsch_stufe = 0;
  s_auftrag_bild.steht_an = false;
  s_auftrag_stufe.steht_an = false;
}

#endif  // CONFIG_BOULDER_HEALTH_APP
