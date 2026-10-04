/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "pbl/services/comm_session/session.h"
#include "pbl/services/health_sync_endpoint.h"
#include "pbl/services/system_task.h"

#include "stubs_logging.h"

// Boulder: der Empfang der Health-Sync-Anfrage des Telefons (Upstream-Code). Nur
// der Parser - die Verbindung selbst ist nicht Teil davon.

void health_sync_protocol_msg_callback(CommSession *session, const uint8_t *msg, size_t len);

static int s_auftraege;
bool system_task_add_callback(SystemTaskEventCallback cb, void *data) {
  s_auftraege++;
  return true;
}
bool system_task_add_callback_droppable(SystemTaskEventCallback cb, void *data) {
  s_auftraege++;
  return true;
}
CommSession *comm_session_get_system_session(void) {
  return NULL;
}
bool comm_session_send_data(CommSession *session, uint16_t endpoint_id, const uint8_t *data,
                            size_t length, uint32_t timeout_ms) {
  return true;
}
bool activity_tracking_on(void) {
  return false;
}
void activity_algorithm_send_minutes(void) {
}
void dls_send_all_sessions(void) {
}

void test_health_sync_endpoint__initialize(void) {
  s_auftraege = 0;
}

// EINE LEERE NACHRICHT HAT KEINEN BEFEHL: frueher wurde das Byte dahinter
// trotzdem gelesen. Mit NULL als Nachricht stuerzte das ab.
void test_health_sync_endpoint__leere_nachricht_wird_nicht_gelesen(void) {
  health_sync_protocol_msg_callback(NULL, NULL, 0);
  cl_assert_equal_i(s_auftraege, 0);
}

// Eine richtige Anfrage geht weiter an KernelBG; eine zu kurze nicht.
void test_health_sync_endpoint__anfrage_geht_an_kernelbg(void) {
  const uint8_t anfrage[] = { 0x01, 0x10, 0x00, 0x00, 0x00 };
  health_sync_protocol_msg_callback(NULL, anfrage, sizeof(anfrage));
  cl_assert_equal_i(s_auftraege, 1);
  health_sync_protocol_msg_callback(NULL, anfrage, 1);
  cl_assert_equal_i(s_auftraege, 1);
}
