/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "applib/graphics/framebuffer.h"
#include "applib/graphics/graphics.h"
#include "apps/system/boulder_health/daten.h"
#include "apps/system/boulder_tagesband/bild.h"
#include "clar.h"
#include "fake_spi_flash.h"
#include "fixtures/load_test_resources.h"
#include "fw/graphics/util.h"
#include "pbl/util/size.h"
#include "stubs_app_state.h"
#include "stubs_compiled_with_legacy2_sdk.h"
#include "stubs_logging.h"
#include "stubs_memory_layout.h"
#include "stubs_pbl_malloc.h"

#include <string.h>

// Das Zifferblatt Tagesband, gezeichnet mit Pebbles echter Grafik und Schrift und mit Bildern
// verglichen: am Tag, mit 12 h, im Schlafmodus, unter der Timeline-Vorschau und leer nach dem Boot.
//
// DIE BILDER SIND GESEHEN, NICHT ERRECHNET: Ein neues Bild schreibt der Test als "-actual" in den
// Ausgabeordner; erst nach dem Ansehen kommt es nach tests/test_images/.

static GContext s_ctx;
static FrameBuffer s_fb;
static BhDaten s_d;

// Ortszeit wie die Pruefdaten: Mitteleuropa im Sommer (daten.c rechnet sie mit sys_localtime_r).
int32_t bh_ortsversatz(time_t utc) {
  return 2 * 60 * 60;
}

GContext *graphics_context_get_current_context(void) {
  return &s_ctx;
}

// --- Pruefdaten: 12.10.2026 nach der Nacht 22:17-07:00 MESZ ---

#define JETZT        1791806700 // 12.10.2026 12:05 UTC = 14:05 MESZ
#define NACHT_BEGINN 1791748800 // 11.10.2026 20:00 UTC = 22:00 MESZ
#define HEUTE        9781
#define ORT          (2 * 60 * 60)

// Dieselbe Nacht wie test_bh_karten: vier Zyklen mit kurzen Wachphasen.
static void prv_nacht(NachtBild *n) {
  memset(n, 0, sizeof(*n));
  n->beginn_utc = NACHT_BEGINN;
  n->utc_to_local = ORT;
  n->einschlafen = 17;
  n->aufwachen = 540;
  n->schlaf_minuten = 472;
  n->hrv = 56;
  n->ruhepuls = 52;
  n->fertig = true;
  n->version = NACHT_BILD_VERSION;
  static const struct {
    uint16_t dauer;
    uint8_t phase;
  } folge[] = {
    {12, 0}, {35, 1}, {30, 2}, {40, 1}, {18, 3}, {5, 0}, {45, 1}, {24, 2}, {30, 1}, {25, 3},
    {6, 0},  {50, 1}, {20, 2}, {40, 1}, {30, 3}, {8, 0}, {47, 1}, {38, 3}, {32, 0},
  };
  uint16_t m = 17 - 12;
  for (size_t i = 0; i < ARRAY_LENGTH(folge); i++) {
    n->abschnitte[i] =
        (NachtDlsAbschnitt){.von = m, .bis = m + folge[i].dauer, .phase = folge[i].phase};
    m += folge[i].dauer;
  }
  n->anzahl = ARRAY_LENGTH(folge);
}

// Alles da, zur Zeit `jetzt`: das Herzband bis dahin, nachts um 55, tags um 75, um 8 Uhr ein Lauf.
// Um 10 und um 11 Uhr je eine halbe Stunde ohne Puls (Uhr abgelegt): dazwischen steht eine
// Halbstunde allein.
static void prv_voll(time_t jetzt) {
  BhDaten *d = &s_d;
  memset(d, 0, sizeof(*d));
  d->jetzt = jetzt;
  d->heute = HEUTE;
  d->minute = (uint16_t)(((jetzt + ORT) % 86400) / 60);
  d->h24 = true;
  d->lage_da = true;
  d->lage = (BoulderLage){
    .fassung = BOULDER_LAGE_FASSUNG,
    .tracking = true,
    .messstufe = 3,
    .nacht_stufe = 3,
    .stand_utc = jetzt,
    .ziel_schritte = 10000,
    .ziel_schlaf_min = 480,
    .maxpuls = 187,
    .ruhepuls_median = 52
  };
  d->nacht_lesen = BoulderLesenDa;
  prv_nacht(&d->nacht);

  d->naechte_da = true;
  d->naechte.fassung = BOULDER_NAECHTE_FASSUNG;
  d->naechte.anzahl = 7;
  d->naechte.neu = 6;
  for (int i = 0; i < 7; i++) {
    const int alter = 6 - i;
    d->naechte.tage[i] = (BoulderNachtEintrag){
      .tag = (uint16_t)(HEUTE - alter),
      .hrv = (uint16_t)(alter == 0 ? 56 : 46 + alter),
      .ruhepuls = (uint8_t)(alter == 0 ? 52 : 53 + alter % 2)
    };
  }

  d->herz_da = true;
  d->herz.neueste = (uint32_t)(jetzt / HERZ_HALBSTUNDE_S);
  for (int k = 0; k < HERZ_HALBSTUNDEN; k++) {
    const uint32_t nr = d->herz.neueste - (HERZ_HALBSTUNDEN - 1) + k;
    const int stunde_ort = (int)(((nr * HERZ_HALBSTUNDE_S + ORT) % 86400) / 3600);
    int mittel = (stunde_ort >= 22 || stunde_ort < 7) ? 55 + k % 3 : 72 + (k * 5) % 11;
    if (stunde_ort == 8) {
      mittel = 128 + k % 2 * 18;
    }
    const uint8_t p = nr % HERZ_HALBSTUNDEN;
    const int halbe_ort = (int)(((nr * HERZ_HALBSTUNDE_S + ORT) % 86400) / HERZ_HALBSTUNDE_S);
    if (halbe_ort == 20 || halbe_ort == 22) {
      continue;
    }
    d->herz.mittel[p] = (uint8_t)mittel;
    d->herz.min[p] = (uint8_t)(mittel - 6);
    d->herz.max[p] = (uint8_t)(mittel + 9);
    d->herz.n[p] = 25;
  }

  d->schritte = 6230;

  // Von heute: Spaziergang 08:15, Training 11:00, Nickerchen 13:00; dazu Pebbles Schlaf der Nacht,
  // den das Band nicht zeigt.
  d->sitzungen_anzahl = 4;
  d->sitzungen[0] = (ActivitySession){
    .start_utc = NACHT_BEGINN + 17 * 60,
    .length_min = 523,
    .type = ActivitySessionType_Sleep
  };
  d->sitzungen[1] = (ActivitySession){
    .start_utc = JETZT - 350 * 60,
    .length_min = 32,
    .type = ActivitySessionType_Walk
  };
  d->sitzungen[2] = (ActivitySession){
    .start_utc = JETZT - 185 * 60,
    .length_min = 41,
    .type = ActivitySessionType_Open
  };
  d->sitzungen[3] = (ActivitySession){
    .start_utc = JETZT - 65 * 60,
    .length_min = 20,
    .type = ActivitySessionType_Nap
  };
}

void test_tagesband_bild__initialize(void) {
  framebuffer_init(&s_fb, &(GSize){DISP_COLS, DISP_ROWS});
  framebuffer_clear(&s_fb);
  graphics_context_init(&s_ctx, &s_fb, GContextInitializationMode_App);
  s_app_state_get_graphics_context = &s_ctx;
  fake_spi_flash_init(0 /* offset */, 0x1000000 /* length */);
  pfs_init(false /* run filesystem check */);
  pfs_format(true /* write erase headers */);
  load_resource_fixture_in_flash(RESOURCES_FIXTURE_PATH, SYSTEM_RESOURCES_FIXTURE_NAME,
                                 false /* is_next */);
  resource_init();
  prv_voll(JETZT);
}

void test_tagesband_bild__cleanup(void) {
}

// Ein Makro statt einer Funktion: TEST_PBI_FILE nennt das Bild nach der Testfunktion (__func__).
#define BILD(verdeckt)                                                        \
  do {                                                                        \
    bt_zeichnen(&s_ctx, GRect(0, 0, DISP_COLS, DISP_ROWS), (verdeckt), &s_d); \
    cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));              \
  } while (0)

void test_tagesband_bild__tag(void) {
  BILD(false);
}

void test_tagesband_bild__zwoelf_stunden(void) {
  s_d.h24 = false;
  BILD(false);
}

// Die breiteste Uhrzeit mit 12 h: zwei Ziffern vor dem Doppelpunkt und "PM".
void test_tagesband_bild__zwoelf_stunden_breit(void) {
  prv_voll(1791838680); // 22:58 MESZ
  s_d.h24 = false;
  BILD(false);
}

void test_tagesband_bild__schlafmodus(void) {
  prv_voll(1791839700);                   // 23:15 MESZ
  s_d.lage.schlafmodus_seit = 1791837600; // 22:40 MESZ
  BILD(false);
}

void test_tagesband_bild__schlafmodus_zwoelf_stunden(void) {
  prv_voll(1791839700);
  s_d.lage.schlafmodus_seit = 1791837600;
  s_d.h24 = false;
  BILD(false);
}

void test_tagesband_bild__verdeckt(void) {
  BILD(true);
}

// Frisch nach dem Boot: keine Nacht, kein Herz, keine Sitzung, noch keine Lage.
void test_tagesband_bild__leer(void) {
  memset(&s_d, 0, sizeof(s_d));
  s_d.jetzt = JETZT;
  s_d.heute = HEUTE;
  s_d.minute = 14 * 60 + 5;
  s_d.h24 = true;
  s_d.nacht_lesen = BoulderLesenSpaeter;
  BILD(false);
}
