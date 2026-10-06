/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "pbl/services/activity/activity.h"
#include "pbl/services/activity/activity_private.h"
#include "pbl/services/activity/boulder/nacht_satz.h"
#include "pbl/services/data_logging/data_logging_service.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// DIE GETEILTEN FORMATE - wie Boulder (Brueckenfassung 0.50) sie liest. Der
// Aufbau ist schon im Bau festgehalten (Static-Asserts in nacht_satz.h und
// vertraege.c); hier steht er Byte fuer Byte, little endian wie auf der Uhr.

int boulder_vertrag_typen(ActivitySessionType typ);
int boulder_vertrag_tags(DlsSystemTag tag);
int boulder_vertrag_tags_vergeben(uint32_t tag);

void test_boulder_vertraege__sitzungstypen_wie_pebble(void) {
  cl_assert_equal_i(ActivitySessionType_Sleep, 1);
  cl_assert_equal_i(ActivitySessionType_RestfulSleep, 2);
  cl_assert_equal_i(ActivitySessionType_Nap, 3);
  cl_assert_equal_i(ActivitySessionType_RestfulNap, 4);
  cl_assert_equal_i(ActivitySessionType_Walk, 5);
  cl_assert_equal_i(ActivitySessionType_Run, 6);
  cl_assert_equal_i(ActivitySessionType_Open, 7);
  cl_assert_equal_i(ActivitySessionTypeCount, 8);
  cl_assert_equal_i(boulder_vertrag_typen(ActivitySessionType_Open), 7);
}

void test_boulder_vertraege__tag_84_fassung_3(void) {
  cl_assert_equal_i(DlsSystemTagActivitySession, 84);
  cl_assert_equal_i(ACTIVITY_SESSION_LOGGING_VERSION, 3);
  ActivitySessionDataLoggingRecord r;
  memset(&r, 0, sizeof(r));
  r.version = 0x0102;
  r.activity = ActivitySessionType_Walk;
  r.utc_to_local = 7200;
  r.start_utc = 0x11223344;
  r.elapsed_sec = 600;
  r.step_data.steps = 0x0A0B;
  const uint8_t *b = (const uint8_t *)&r;
  cl_assert_equal_i(sizeof(r), 26);
  cl_assert_equal_i(b[0], 0x02);
  cl_assert_equal_i(b[4], ActivitySessionType_Walk);
  cl_assert_equal_i(b[6], 7200 & 0xFF);
  cl_assert_equal_i(b[10], 0x44);
  cl_assert_equal_i(b[13], 0x11);
  cl_assert_equal_i(b[14], 600 & 0xFF);
  cl_assert_equal_i(b[18], 0x0B);
  cl_assert_equal_i(b[19], 0x0A);
}

void test_boulder_vertraege__boulder_tags(void) {
  cl_assert_equal_i(BOULDER_DLS_TAG_NACHT, 0x4001);
  cl_assert_equal_i(BOULDER_DLS_TAG_NACHT_HRV, 0x4002);
  // 88 und 89 bleiben gesperrt; kein Upstream-Tag liegt dort.
  cl_assert_equal_i(boulder_vertrag_tags_vergeben(88), 1);
  cl_assert_equal_i(boulder_vertrag_tags_vergeben(89), 1);
  cl_assert_equal_i(boulder_vertrag_tags_vergeben(0x4001), 1);
  cl_assert_equal_i(boulder_vertrag_tags_vergeben(90), 0);
  cl_assert_equal_i(boulder_vertrag_tags(DlsSystemTagActivitySession), 84);
}

void test_boulder_vertraege__nachtsatz_byte_fuer_byte(void) {
  NachtDlsRecord r;
  memset(&r, 0, sizeof(r));
  r.version = NACHT_DLS_VERSION;
  r.size = sizeof(r);
  r.beginn_utc = 0x6AB8C0A0;
  r.utc_to_local = 7200;
  r.einschlafen = 137;
  r.aufwachen = 660;
  r.schlaf_minuten = 0x01F2;
  r.ruhepuls = 54;
  r.teil = 1;
  r.teile = 2;
  r.hrv = 0x0135;
  r.anzahl = 2;
  r.abschnitte[0] = (NachtDlsAbschnitt){ .von = 0x0089, .bis = 0x0100, .phase = 1 };
  r.abschnitte[59] = (NachtDlsAbschnitt){ .von = 0x0293, .bis = 0x0294, .phase = 3 };
  r.akku_beginn = 78;
  r.akku_ende = 69;
  r.flags = NACHT_DLS_FLAG_GELADEN;
  r.messstufe = 4;
  const uint8_t soll_kopf[24] = {
    0x03, 0x00, 0x48, 0x01, 0xA0, 0xC0, 0xB8, 0x6A, 0x20, 0x1C, 0x00, 0x00,
    0x89, 0x00, 0x94, 0x02, 0xF2, 0x01, 54, 1, 2, 0x35, 0x01, 2,
  };
  const uint8_t *b = (const uint8_t *)&r;
  cl_assert_equal_i(memcmp(b, soll_kopf, sizeof(soll_kopf)), 0);
  const uint8_t soll_a0[5] = { 0x89, 0x00, 0x00, 0x01, 1 };
  cl_assert_equal_i(memcmp(b + 24, soll_a0, 5), 0);
  const uint8_t soll_a59[5] = { 0x93, 0x02, 0x94, 0x02, 3 };
  cl_assert_equal_i(memcmp(b + 24 + 59 * 5, soll_a59, 5), 0);
  const uint8_t soll_ende[4] = { 78, 69, 1, 4 };
  cl_assert_equal_i(memcmp(b + 324, soll_ende, 4), 0);
}

void test_boulder_vertraege__hrv_satz_byte_fuer_byte(void) {
  NachtHrvDlsRecord r;
  memset(&r, 0, sizeof(r));
  r.version = NACHT_HRV_DLS_VERSION;
  r.size = sizeof(r);
  r.beginn_utc = 0x01020304;
  r.teil = 0;
  r.teile = 1;
  r.anzahl = 1;
  r.fenster[47] = (NachtHrvDlsFenster){ .minute = 0x0201, .rmssd = 0x0043, .puls = 58 };
  const uint8_t soll_kopf[11] = { 0x01, 0x00, 0xFB, 0x00, 0x04, 0x03, 0x02, 0x01, 0, 1, 1 };
  const uint8_t *b = (const uint8_t *)&r;
  cl_assert_equal_i(memcmp(b, soll_kopf, sizeof(soll_kopf)), 0);
  const uint8_t soll_f47[5] = { 0x01, 0x02, 0x43, 0x00, 58 };
  cl_assert_equal_i(memcmp(b + 11 + 47 * 5, soll_f47, 5), 0);
}

void test_boulder_vertraege__nachtbild_wie_b24(void) {
  cl_assert_equal_i(sizeof(NachtBild), 512);
  cl_assert_equal_i(offsetof(NachtBild, minuten), 14);
  cl_assert_equal_i(offsetof(NachtBild, hrv), 24);
  cl_assert_equal_i(offsetof(NachtBild, fertig), 27);
  cl_assert_equal_i(offsetof(NachtBild, anzahl), 28);
  cl_assert_equal_i(offsetof(NachtBild, abschnitte), 29);
  cl_assert_equal_i(offsetof(NachtBild, version), 509);
}

// DIE REFERENZSAETZE AUS b26 (tests/fixtures/activity/boulder/referenz-b26, AP-G)
// passen in V2s Formate: Kopf, Teile und Abschnitte lesbar, und die Phasen je
// Minute ergeben sich aus dem letzten Satz der Nacht.
static const char *s_referenzen[] = {
  "nacht-2026-09-26", "nacht-2026-09-29", "nacht-2026-09-30",
  "nacht-2026-10-03", "nacht-2026-10-05", "nacht-2026-10-05-morgen",
};

static void prv_hex(const char *hex, void *aus, size_t n) {
  cl_assert_equal_i(strlen(hex), 2 * n);
  uint8_t *b = aus;
  for (size_t k = 0; k < n; k++) {
    char z[3] = { hex[2 * k], hex[2 * k + 1], 0 };
    b[k] = (uint8_t)strtoul(z, NULL, 16);
  }
}

void test_boulder_vertraege__referenzsaetze_aus_b26_passen(void) {
  static char zeile[4096];
  for (size_t r = 0; r < sizeof(s_referenzen) / sizeof(s_referenzen[0]); r++) {
    char pfad[512];
    snprintf(pfad, sizeof(pfad), "%s/activity/boulder/referenz-b26/%s.txt", CLAR_FIXTURE_PATH,
             s_referenzen[r]);
    FILE *f = fopen(pfad, "r");
    cl_assert(f);
    long beginn = 0;
    int minuten = 0, saetze = 0, hrv_saetze = 0, bilder = 0;
    NachtDlsRecord letzte[4];
    int teile = 0;
    char phasen[1200] = { 0 };
    while (fgets(zeile, sizeof(zeile), f)) {
      zeile[strcspn(zeile, "\r\n")] = 0;
      char *wert = strchr(zeile, ' ');
      if (zeile[0] == '#' || !wert) {
        continue;
      }
      *wert++ = 0;
      if (!strcmp(zeile, "beginn_utc")) {
        beginn = atol(wert);
      } else if (!strcmp(zeile, "minuten")) {
        minuten = atoi(wert);
      } else if (!strcmp(zeile, "satz4001")) {
        NachtDlsRecord s;
        prv_hex(strchr(wert, ' ') + 1, &s, sizeof(s));
        cl_assert_equal_i(s.version, NACHT_DLS_VERSION);
        cl_assert_equal_i(s.size, sizeof(NachtDlsRecord));
        cl_assert_equal_i(s.beginn_utc, beginn);
        cl_assert_equal_i(s.utc_to_local, 2 * 60 * 60);
        cl_assert(s.teil < s.teile && s.anzahl <= NACHT_DLS_ABSCHNITTE);
        cl_assert(s.einschlafen < s.aufwachen && s.messstufe >= 1 && s.messstufe <= 5);
        for (int a = 0; a < s.anzahl; a++) {
          cl_assert(s.abschnitte[a].von < s.abschnitte[a].bis && s.abschnitte[a].phase < 5);
          cl_assert(a == 0 || s.abschnitte[a].von == s.abschnitte[a - 1].bis);
        }
        if (s.teil == 0) {
          teile = 0;
        }
        cl_assert(teile < 4);
        letzte[teile++] = s;
        saetze++;
      } else if (!strcmp(zeile, "satz4002")) {
        NachtHrvDlsRecord h;
        prv_hex(strchr(wert, ' ') + 1, &h, sizeof(h));
        cl_assert_equal_i(h.version, NACHT_HRV_DLS_VERSION);
        cl_assert_equal_i(h.size, sizeof(NachtHrvDlsRecord));
        cl_assert_equal_i(h.beginn_utc, beginn);
        cl_assert(h.anzahl > 0 && h.anzahl <= NACHT_HRV_DLS_FENSTER);
        hrv_saetze++;
      } else if (!strcmp(zeile, "bild")) {
        NachtBild b;
        prv_hex(wert, &b, sizeof(b));
        cl_assert_equal_i(b.version, NACHT_BILD_VERSION);
        cl_assert(b.fertig);
        cl_assert_equal_i(b.beginn_utc, beginn);
        bilder++;
      } else if (!strcmp(zeile, "phasen")) {
        cl_assert(strlen(wert) < sizeof(phasen));
        strcpy(phasen, wert);
      }
    }
    fclose(f);
    cl_assert(saetze > 0 && bilder == 1 && minuten > 0);
    cl_assert_equal_i(strlen(phasen), minuten);
    cl_assert_equal_i(teile, letzte[0].teile);
    // Die Phasen je Minute folgen aus dem letzten Satz.
    char soll[1200];
    memset(soll, '.', minuten);
    soll[minuten] = 0;
    int schlaf = 0;
    for (int t = 0; t < teile; t++) {
      for (int a = 0; a < letzte[t].anzahl; a++) {
        for (int m = letzte[t].abschnitte[a].von; m < letzte[t].abschnitte[a].bis; m++) {
          const int i = (int)((letzte[t].beginn_utc - beginn) / 60) + m;
          if (i >= 0 && i < minuten) {
            soll[i] = '0' + letzte[t].abschnitte[a].phase;
            schlaf += (letzte[t].abschnitte[a].phase != 0);
          }
        }
      }
    }
    cl_assert_equal_s(phasen, soll);
    cl_assert_equal_i(schlaf, letzte[0].schlaf_minuten);
    printf("\n%s: %d Saetze 0x4001, %d Saetze 0x4002, Schlaf %d min", s_referenzen[r], saetze,
           hrv_saetze, schlaf);
  }
}
