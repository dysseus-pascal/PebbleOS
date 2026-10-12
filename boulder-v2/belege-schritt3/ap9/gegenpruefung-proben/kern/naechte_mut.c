/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// Boulder V2, Schritt 3: HRV und Ruhepuls je Wachtag (naechte.h, Spezifikation 3.8.1, AP3). Nur
// mit der eigenen Health-App; ohne den Schalter bleibt die Datei leer.
#ifdef CONFIG_BOULDER_HEALTH_APP

#include "pbl/services/activity/boulder/naechte.h"

#include "pbl/kernel/mutex.h"
#include "pbl/services/activity/boulder/speicher.h"
#include "pbl/services/time.h"
#include "pbl/util/testing.h"
#include "pbl/util/units.h"
#include <pbl/logging/logging.h>

#include <string.h>

PBL_LOG_MODULE_DECLARE(service_activity, CONFIG_SERVICE_ACTIVITY_LOG_LEVEL);

static const char SCHLUESSEL[] = "naechte";
// Ohne Nullbyte, wie "ruhe" und "kopf" (speicher.c).
#define SCHLUESSEL_LEN (sizeof(SCHLUESSEL) - 1)

// 1.1.2000 0:00 als Ortszeit in Sekunden seit 1970: der Tag 0 der Wachtage (wie das Orakel aus
// Boulder, LocalDate.toEpochDay() - 10957).
#define TAG_NULL_LOKAL 946684800

// EINE BLATTSPERRE (3.4): Unter ihr wird nur kopiert und verglichen, nichts gerufen. Fuer die Tests
// sichtbar (Sperrfolge, T2.4).
PBL_T_STATIC PBL_MUTEX_DEFINE(s_naechte_sperre);
static BoulderNaechte s_naechte;
static bool s_geladen; //< "Laden" hat die Liste seit dem Boot gelesen (unter der Sperre)
static bool s_faellig; //< geaendert, noch nicht in der Datei (nur KernelBG)

// Platz des Eintrags, der `alter` Wachtage hinter dem juengsten liegt (0 = der juengste).
static int prv_platz(const BoulderNaechte *n, int alter) {
  return (n->neu + NAECHTE_MAX - alter) % NAECHTE_MAX;
}

// Was aus der Datei kommt, muss der Ring sein, den eintragen erwartet: sonst schoebe er Eintraege
// ueber das Feld hinaus oder sortierte falsch ein.
static bool prv_gueltig(const BoulderNaechte *n) {
  if (n->fassung != BOULDER_NAECHTE_FASSUNG || n->anzahl > NAECHTE_MAX || n->neu >= NAECHTE_MAX) {
    return false;
  }
  for (int i = 1; i < n->anzahl; i++) {
    if (n->tage[prv_platz(n, i)].tag >= n->tage[prv_platz(n, i - 1)].tag) {
      return false;
    }
  }
  return true;
}

// Unter der Sperre: Rueckgabe, ob sich die Liste geaendert hat.
static bool prv_eintragen(BoulderNaechte *n, uint16_t tag, uint16_t schlaf_min, uint16_t hrv,
                          uint8_t ruhepuls) {
  const BoulderNachtEintrag neu = {.tag = tag, .hrv = hrv, .ruhepuls = ruhepuls};
  if (n->anzahl == 0 || tag > n->tage[n->neu].tag) {
    // Ein neuer juengster Wachtag; ist die Liste voll, faellt der aelteste.
    n->neu = n->anzahl ? (uint8_t)((n->neu + 1) % NAECHTE_MAX) : 0;
    if (n->anzahl < NAECHTE_MAX) {
      n->anzahl++;
    }
    n->tage[n->neu] = neu;
    n->min_ruhe = schlaf_min;
    n->min_hrv = hrv ? schlaf_min : 0;
    return true;
  }
  if (tag == n->tage[n->neu].tag) {
    // Noch eine Nacht des juengsten Wachtags (Folgenacht, Nacht vor Mitternacht): die laengere
    // gilt, bei gleicher Laenge die fruehere (jeTag); die HRV nur aus Naechten mit HRV
    // (jeTagMitHrv). Beide Regeln getrennt, wie in Boulder.
    BoulderNachtEintrag *e = &n->tage[n->neu];
    bool geaendert = false;
    if (schlaf_min > n->min_ruhe) {
      e->ruhepuls = ruhepuls;
      n->min_ruhe = schlaf_min;
      geaendert = true;
    }
    if (hrv && schlaf_min > n->min_hrv) {
      e->hrv = hrv;
      n->min_hrv = schlaf_min;
      geaendert = true;
    }
    return geaendert;
  }
  // Ein aelterer Wachtag (die Uhr ging zurueck): einsortieren, wenn er fehlt (naechte.h).
  int alter = 0;
  while (alter < n->anzahl && n->tage[prv_platz(n, alter)].tag > tag) {
    alter++;
  }
  if (alter == NAECHTE_MAX || (alter < n->anzahl && n->tage[prv_platz(n, alter)].tag == tag)) {
    return false; // aelter als alle 37, oder der Tag ist schon da
  }
  // Die aelteren ruecken einen Platz nach hinten; ist die Liste voll, faellt der aelteste.
  const int letzter = (n->anzahl < NAECHTE_MAX) ? n->anzahl : NAECHTE_MAX - 1;
  for (int i = letzter; i > alter; i--) {
    n->tage[prv_platz(n, i)] = n->tage[prv_platz(n, i - 1)];
  }
  n->tage[prv_platz(n, alter)] = neu;
  if (n->anzahl < NAECHTE_MAX) {
    n->anzahl++;
  }
  return true;
}

void boulder_naechte_eintragen(time_t aufwachen_utc, uint16_t schlaf_min, uint16_t hrv,
                               uint8_t ruhepuls) {
  // DAS ORTSDATUM DES AUFWACHENS, mit der Zeitzone samt Sommerzeit zu diesem Zeitpunkt (Boulder:
  // Instant.atZone). Ausserhalb der Sperre: Das ist Pebbles Code (3.4).
  const time_t lokal = time_utc_to_local(aufwachen_utc);
  if (lokal < TAG_NULL_LOKAL) {
    PBL_LOG_WRN("Boulder: Nacht ohne gueltige Uhrzeit, nicht eingetragen");
    return;
  }
  const uint32_t tage = (uint32_t)((lokal - TAG_NULL_LOKAL) / PBL_SEC_PER_DAY);
  const uint16_t tag = (tage > UINT16_MAX) ? UINT16_MAX : (uint16_t)tage;
  // Vor "Laden" kommt keine Nacht hierher: Bis dahin fuettert der Minutenteil nur den Eingang
  // (nacht.c). Und "Laden" liest die Liste ohnehin neu aus der Datei.
  pbl_mutex_lock(&s_naechte_sperre, PBL_FOREVER);
  const bool geaendert = prv_eintragen(&s_naechte, tag, schlaf_min, hrv, ruhepuls);
  const uint8_t anzahl = s_naechte.anzahl;
  pbl_mutex_unlock(&s_naechte_sperre);
  s_faellig |= geaendert;
  PBL_LOG_INFO("Boulder: Nacht fuer Wachtag %d (%d min, HRV %d, Ruhepuls %d)%s, %d Tage", (int)tag,
               (int)schlaf_min, (int)hrv, (int)ruhepuls, geaendert ? "" : " aendert nichts",
               (int)anzahl);
}

void boulder_naechte_saat(const NachtBild *bild) {
  boulder_naechte_eintragen((time_t)bild->beginn_utc + (time_t)bild->aufwachen * PBL_SEC_PER_MIN,
                            bild->schlaf_minuten, bild->hrv, bild->ruhepuls);
}

bool boulder_naechte_kopieren(BoulderNaechte *aus) {
  // UNTER DER SPERRE NUR DIE KOPIE (3.1, Regel 3), direkt in den Puffer des Aufrufers.
  pbl_mutex_lock(&s_naechte_sperre, PBL_FOREVER);
  const bool geladen = s_geladen;
  if (geladen) {
    memcpy(aus, &s_naechte, sizeof(*aus));
  }
  pbl_mutex_unlock(&s_naechte_sperre);
  if (!geladen) {
    memset(aus, 0, sizeof(*aus));
    aus->fassung = BOULDER_NAECHTE_FASSUNG;
  }
  return geladen;
}

void boulder_naechte_laden(SettingsFile *datei) {
  pbl_mutex_lock(&s_naechte_sperre, PBL_FOREVER);
  s_geladen = false;
  // MUTANT: Datei unter der Sperre
  // DIREKT IN DIE LISTE, OHNE SPERRE UND OHNE PUFFER: Unter der Sperre darf keine Datei gelesen
  // werden (3.4), und 230 B mehr auf dem Stapel von "Laden" vertruege die Kette nicht (T7.3).
  // Solange s_geladen falsch ist, kopiert S5 nichts aus ihr, und nur KernelBG schreibt sie.
  const int laenge = datei ? settings_file_get_len(datei, SCHLUESSEL, SCHLUESSEL_LEN) : 0;
  const bool gelesen = laenge == (int)sizeof(s_naechte) &&
                       settings_file_get(datei, SCHLUESSEL, SCHLUESSEL_LEN, &s_naechte,
                                         sizeof(s_naechte)) == S_SUCCESS &&
                       prv_gueltig(&s_naechte);
  if (!gelesen) {
    if (laenge > 0) {
      PBL_LOG_WRN("Boulder: Naechte in %s ungueltig (%d B), die Liste beginnt neu", SPEICHER_DATEI,
                  laenge);
    }
    memset(&s_naechte, 0, sizeof(s_naechte));
    s_naechte.fassung = BOULDER_NAECHTE_FASSUNG;
  }
  s_faellig = false;
  s_geladen = true;
  pbl_mutex_unlock(&s_naechte_sperre);
  PBL_LOG_INFO("Boulder: %d Naechte geladen", (int)s_naechte.anzahl);
}

void boulder_naechte_ablegen(SettingsFile *datei) {
  if (!s_faellig) {
    return;
  }
  // Ohne Kopie: Die Ablage laeuft auf KernelBG, und nur KernelBG aendert die Liste.
  pbl_mutex_lock(&s_naechte_sperre, PBL_FOREVER);
  const status_t st_mut = settings_file_set(datei, SCHLUESSEL, SCHLUESSEL_LEN, &s_naechte, sizeof(s_naechte));
  pbl_mutex_unlock(&s_naechte_sperre);
  if (st_mut !=
      S_SUCCESS) {
    PBL_LOG_WRN("Boulder: Naechte nicht gespeichert, naechste Ablage wieder");
    return;
  }
  s_faellig = false;
}

const BoulderNaechte *boulder_naechte_kern(void) {
  return &s_naechte;
}

void boulder_naechte_test_neustart(void) {
  memset(&s_naechte, 0, sizeof(s_naechte));
  s_geladen = false;
  s_faellig = false;
}

#endif // CONFIG_BOULDER_HEALTH_APP
