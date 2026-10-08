/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// Boulder V2, Schritt 3: die Ziele (ziele.h, Spezifikation 3.8.4). Nur mit der eigenen
// Health-App; ohne den Schalter bleibt die Datei leer.
#ifdef CONFIG_BOULDER_HEALTH_APP

#include "pbl/services/activity/boulder/ziele.h"

#include "applib/event_service_client.h"
#include "kernel/event_loop.h"
#include "kernel/events.h"
#include "pbl/services/activity/boulder/takt.h"
#include "pbl/services/blob_db/api.h"
#include "system/status_codes.h"
#include <pbl/logging/logging.h>

#include <string.h>

PBL_LOG_MODULE_DECLARE(service_activity, CONFIG_SERVICE_ACTIVITY_LOG_LEVEL);

// Nur KernelBG schreibt und liest die Werte (Auftrag, Lage); das Abo auf KernelMain setzt nur
// die Merker.
static uint16_t s_schritte = ZIEL_SCHRITTE_VORGABE;
static uint16_t s_schlaf_min = ZIEL_SCHLAF_VORGABE;
static bool s_gestartet;  //< erster Auftrag lief, Abo angestossen
static bool s_lesen;      //< neu lesen (Ereignis, Ablage)

static void prv_ziele_cb(void *data);
static BoulderAufgabe s_aufgabe = BOULDER_AUFGABE("Boulder Ziele", prv_ziele_cb);

uint16_t boulder_ziel_schritte(void) {
  return s_schritte;
}

uint16_t boulder_ziel_schlaf_min(void) {
  return s_schlaf_min;
}

bool boulder_ziele_gueltig(const BoulderZieleSatz *satz, int laenge) {
  return laenge == (int)sizeof(*satz) && satz->fassung == BOULDER_ZIELE_FASSUNG &&
         satz->schritte >= ZIEL_SCHRITTE_MIN && satz->schritte <= ZIEL_SCHRITTE_MAX &&
         satz->schlaf_min >= ZIEL_SCHLAF_MIN && satz->schlaf_min <= ZIEL_SCHLAF_MAX;
}

// Datei und Heap (healthdb): nur hier im eigenen Auftrag. Die Laenge zuerst - healthdb nimmt
// jedes Vielfache von 4 (health_db.c, prv_value_is_valid), und settings_file_get mit 8 B laese von
// einem laengeren Satz nur den Anfang.
static void prv_lesen(void) {
  const uint8_t *schluessel = (const uint8_t *)BOULDER_ZIELE_SCHLUESSEL;
  const int schluessel_len = strlen(BOULDER_ZIELE_SCHLUESSEL);
  BoulderZieleSatz satz = { 0 };
  const int laenge = blob_db_get_len(BlobDBIdHealth, schluessel, schluessel_len);
  const bool gelesen =
      laenge == (int)sizeof(satz) &&
      blob_db_read(BlobDBIdHealth, schluessel, schluessel_len, (uint8_t *)&satz,
                   sizeof(satz)) == S_SUCCESS;
  uint16_t schritte = ZIEL_SCHRITTE_VORGABE;
  uint16_t schlaf = ZIEL_SCHLAF_VORGABE;
  if (gelesen && boulder_ziele_gueltig(&satz, laenge)) {
    schritte = satz.schritte;
    schlaf = satz.schlaf_min;
  } else if (laenge > 0) {
    PBL_LOG_WRN("Boulder: Ziele ungueltig (%d B, Fassung %d), es gilt die Vorgabe", laenge,
                gelesen ? satz.fassung : -1);
  }
  if (schritte != s_schritte || schlaf != s_schlaf_min) {
    PBL_LOG_INFO("Boulder: Ziele %d Schritte, %d min Schlaf", schritte, schlaf);
  }
  s_schritte = schritte;
  s_schlaf_min = schlaf;
}

static bool prv_unser_schluessel(const PebbleBlobDBEvent *b) {
  const int n = (int)strlen(BOULDER_ZIELE_SCHLUESSEL);
  // Mit oder ohne Nullbyte: libpebble3 schickt den Schluessel ohne (HealthStat.key()). Ein
  // laengerer Schluessel, der nur so anfaengt, ist ein anderer.
  return b->key && (b->key_len == n || (b->key_len == n + 1 && b->key[n] == '\0')) &&
         memcmp(b->key, BOULDER_ZIELE_SCHLUESSEL, n) == 0;
}

// KernelMain. EIN EIGENER FILTER, nicht der der Messstufe (messstufe.c): Der laesst nur ein
// Einfuegen mit Schluessel durch. Fuer die Ziele zaehlen auch das Loeschen des Schluessels und das
// Leeren der ganzen Datenbank Health, das keinen Schluessel traegt (api.c, blob_db_flush) -
// danach gilt sofort die Vorgabe, nicht erst nach der naechsten Ablage.
static void prv_blobdb_ereignis(PebbleEvent *e, void *context) {
  const PebbleBlobDBEvent *b = &e->blob_db;
  if (b->db_id != BlobDBIdHealth) {
    return;
  }
  const bool betrifft = (b->type == BlobDBEventTypeFlush) ||
                        ((b->type == BlobDBEventTypeInsert || b->type == BlobDBEventTypeDelete) &&
                         prv_unser_schluessel(b));
  if (betrifft) {
    s_lesen = true;
    boulder_aufgabe_einreihen(&s_aufgabe);
  }
}

// WARUM DAS ABO AUF KERNELMAIN: wie bei der Messstufe meldet blob_db_insert dort, und ein Abo
// von KernelBG aus endete in WTF (messstufe.c).
static void prv_abo_anmelden(void *data) {
  static EventServiceInfo s_info = {
    .type = PEBBLE_BLOBDB_EVENT,
    .handler = prv_blobdb_ereignis,
  };
  event_service_client_subscribe(&s_info);
}

static void prv_ziele_cb(void *data) {
  BOULDER_AUFGABE_LAEUFT(&s_aufgabe);
  if (!s_gestartet) {
    s_gestartet = true;
    s_lesen = true;
    launcher_task_add_callback(prv_abo_anmelden, NULL);
  }
  if (!s_lesen) {
    return;
  }
  s_lesen = false;
  prv_lesen();
  // Gleich zur App, nicht erst mit der naechsten Minute.
  boulder_lage_veroeffentlichen();
}

void boulder_ziele_minute(void) {
  if (!s_gestartet || s_lesen) {
    boulder_aufgabe_einreihen(&s_aufgabe);
  }
}

void boulder_ziele_nachlesen(void) {
  s_lesen = true;
  boulder_aufgabe_einreihen(&s_aufgabe);
}

void boulder_ziele_test_neustart(void) {
  s_schritte = ZIEL_SCHRITTE_VORGABE;
  s_schlaf_min = ZIEL_SCHLAF_VORGABE;
  s_gestartet = false;
  s_lesen = false;
  s_aufgabe.steht_an = false;
}

#endif  // CONFIG_BOULDER_HEALTH_APP
