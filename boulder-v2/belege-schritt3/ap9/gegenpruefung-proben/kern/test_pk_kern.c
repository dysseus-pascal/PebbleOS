/* Proben der Gegenpruefung (nicht im Repo). Umfeld wie test_boulder_schnittstelle. */
#include "schnittstelle_umfeld.h"

void test_pk_kern__initialize(void) {
  prv_umfeld_beginn();
}

void test_pk_kern__cleanup(void) {
  prv_umfeld_ende();
}

// Ziele mit Nullbyte am Schluessel: der Filter (ziele.c:78) loest den Auftrag aus, prv_lesen
// (ziele.c:50-57) liest aber nur den Schluessel ohne Nullbyte.
void test_pk_kern__ziele_mit_nullbyte(void) {
  prv_kernel_bg();
  prv_eine_minute();
  const uint32_t vorher = prv_laeufe("Boulder Ziele");
  const BoulderZieleSatz satz = { .fassung = 1, .schritte = 9000, .schlaf_min = 420 };
  cl_must_pass(blob_db_insert(BlobDBIdHealth, (const uint8_t *)BOULDER_ZIELE_SCHLUESSEL,
                              strlen(BOULDER_ZIELE_SCHLUESSEL) + 1, (const uint8_t *)&satz,
                              sizeof(satz)));
  prv_kernel_bg();
  const BoulderLage l = prv_lage();
  printf("\nPROBE Ziele mit Nullbyte: Auftrag %lu -> %lu, Lage %d Schritte / %d min",
         (unsigned long)vorher, (unsigned long)prv_laeufe("Boulder Ziele"), l.ziel_schritte,
         l.ziel_schlaf_min);
  // Erst ein gueltiger Satz ohne Nullbyte, dann einer mit: welcher gilt?
  cl_assert_equal_i(prv_ziele_schicken(1, 7500, 390, 8), S_SUCCESS);
  prv_kernel_bg();
  cl_must_pass(blob_db_insert(BlobDBIdHealth, (const uint8_t *)BOULDER_ZIELE_SCHLUESSEL,
                              strlen(BOULDER_ZIELE_SCHLUESSEL) + 1, (const uint8_t *)&satz,
                              sizeof(satz)));
  prv_kernel_bg();
  const BoulderLage l2 = prv_lage();
  printf("\nPROBE erst ohne (7500), dann mit Nullbyte (9000): Lage %d Schritte", l2.ziel_schritte);
}

// Herzband nach einer Uhr, die um mehr als 24 h zurueckgeht (z. B. von falscher Zukunft auf die
// richtige Zeit): Band und Zonen bleiben stehen; der Takt rechnet nichts nach.
void test_pk_kern__herz_uhr_weit_zurueck(void) {
  prv_kernel_bg();
  cl_assert(boulder_herz_bereit());
  const time_t zukunft = s_jetzt + 3 * PBL_SEC_PER_DAY;
  const time_t beginn_z = zukunft - (zukunft % HERZ_HALBSTUNDE_S);
  for (int m = 0; m < 30; m++) {
    prv_minute(beginn_z, m, 2600, 80, true);
  }
  cl_assert(prv_s6());
  const uint32_t neueste_z = s_app_herz->neueste;
  const uint16_t tag_z = s_app_herz->zonen_tag;
  const uint32_t laeufe = prv_laeufe("Boulder Herz nachrechnen");
  // Uhr richtig gestellt: drei Tage zurueck. Zwei Stunden mit Puls 120.
  const time_t jetzt = s_jetzt - (s_jetzt % HERZ_HALBSTUNDE_S);
  for (int m = 0; m < 120; m++) {
    prv_minute(jetzt, m, 2600, 120, true);
  }
  cl_assert(prv_s6());
  const BoulderHerz *h = s_app_herz;
  int belegt_jetzt = 0;
  for (uint32_t nr = (uint32_t)(jetzt / HERZ_HALBSTUNDE_S);
       nr < (uint32_t)(jetzt / HERZ_HALBSTUNDE_S) + 4; nr++) {
    const bool gueltig = nr + (HERZ_HALBSTUNDEN - 1) >= h->neueste && nr <= h->neueste;
    belegt_jetzt += gueltig && h->n[nr % HERZ_HALBSTUNDEN] != 0;
  }
  const uint16_t heute = (uint16_t)((time_utc_to_local(rtc_get_time()) - 946684800) / 86400);
  printf("\nPROBE Uhr 3 Tage zurueck: neueste %lu (vorher %lu, jetzt %lu), zonen_tag %d (vorher %d,"
         " heute %d), Zonen %d %d %d %d, Halbstunden von jetzt im gueltigen Band %d, "
         "Nachrechnen %lu -> %lu",
         (unsigned long)h->neueste, (unsigned long)neueste_z,
         (unsigned long)((jetzt + 119 * 60) / HERZ_HALBSTUNDE_S), h->zonen_tag, tag_z, heute,
         h->zonen_min[0], h->zonen_min[1], h->zonen_min[2], h->zonen_min[3], belegt_jetzt,
         (unsigned long)laeufe, (unsigned long)prv_laeufe("Boulder Herz nachrechnen"));
}
