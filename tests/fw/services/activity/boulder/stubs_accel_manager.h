/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// Pebbles tests/stubs/stubs_accel_manager.h bis v4.40.0, mit den Signaturen von v4.41.0
// (Generation und "more", 1c85cc53). Upstream hat den Stub geloescht, seine Tests stubben jetzt
// selbst; Boulders Tests uebersetzen hrm_manager.c mit und brauchen vom Beschleunigungsdienst
// nichts. hrm_manager.c liest die Ausgabeparameter, darum sind sie gesetzt.

#include <pbl/services/accel_manager.h>

AccelManagerState *sys_accel_manager_data_subscribe(AccelSamplingRate rate,
                                                    AccelDataReadyCallback data_cb, void *context,
                                                    PebbleTask handler_task) {
  return nullptr;
}

bool sys_accel_manager_data_unsubscribe(AccelManagerState *state) {
  return false;
}

uint32_t accel_manager_set_jitterfree_sampling_rate(AccelManagerState *state,
                                                    uint32_t min_rate_mhz) {
  return 0;
}

int sys_accel_manager_set_sample_buffer(AccelManagerState *state, AccelRawData *buffer,
                                        uint32_t samples_per_update) {
  return 0;
}

uint32_t sys_accel_manager_get_num_samples(AccelManagerState *state, uint64_t *timestamp_ms,
                                           uint32_t *generation) {
  *timestamp_ms = 0;
  *generation = 0;
  return 0;
}

bool sys_accel_manager_consume_samples(AccelManagerState *state, uint32_t samples,
                                       uint32_t generation, bool *more) {
  *more = false;
  return false;
}
