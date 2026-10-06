/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// Boulder V2: die Nachfrage auf der Uhr, ohne Telefon (Spezifikation 3.4, F4).
//
// WARUM EIN KERNEL-DIALOG: Er braucht keine Pebble-Zeile und keine registrierte App - wie
// Pebbles Freigabe des Pulses (ble_hrm_sharing_popup.c) legt er sich auf den Modal-Stapel,
// ueber jede App und jedes Zifferblatt. Nur Englisch ("Cycling?", "Driving?" auch in Tram und
// Zug, V5), vorhandenes Symbol, keine neuen Ressourcen.
//
// EIN BESITZER: Alles hier laeuft auf KernelMain. Der Dialog endet auf genau einem Weg, im
// Entladen seines Fensters - nach einer Taste, nach seinem eigenen Ablauf (2 min) oder wenn
// ihn etwas vom Stapel nimmt. Dort geht die Antwort einmal als Merker an den Dauerteil.

#include "pbl/services/activity/boulder/kontext.h"

#include "applib/graphics/gcolor_definitions.h"
#include "applib/ui/dialogs/actionable_dialog.h"
#include "applib/ui/dialogs/dialog.h"
#include "applib/ui/vibes.h"
#include "kernel/event_loop.h"
#include "kernel/ui/modals/modal_manager.h"
#include "pbl/services/notifications/do_not_disturb.h"
#include "pbl/util/units.h"
#include "resource/resource_ids.auto.h"
#include <pbl/logging/logging.h>

#include <stdint.h>

PBL_LOG_MODULE_DECLARE(service_activity, CONFIG_SERVICE_ACTIVITY_LOG_LEVEL);

static ActionableDialog *s_dialog;
static uint8_t s_nr;
static BoulderNachfrageAntwort s_antwort;

// Der eine Ausgang: das Fenster wird entladen.
static void prv_zu(void *context) {
  if (!s_dialog) {
    return;
  }
  s_dialog = NULL;
  boulder_kontext_antwort(s_nr, s_antwort);
}

static void prv_antworten(BoulderNachfrageAntwort antwort) {
  if (!s_dialog) {
    return;
  }
  if (s_antwort == NachfrageOhneAntwort) {
    s_antwort = antwort;
  }
  actionable_dialog_pop(s_dialog);
}

static void prv_ja(ClickRecognizerRef recognizer, void *context) {
  prv_antworten(NachfrageJa);
}

static void prv_nein(ClickRecognizerRef recognizer, void *context) {
  prv_antworten(NachfrageNein);
}

// Oben Ja, unten und Zurueck Nein - wie Pebbles Rueckfragen.
static void prv_tasten(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, prv_ja);
  window_single_click_subscribe(BUTTON_ID_DOWN, prv_nein);
  window_single_click_subscribe(BUTTON_ID_BACK, prv_nein);
}

static void prv_zeigen(void *daten) {
  const uintptr_t d = (uintptr_t)daten;
  const BoulderFahrtArt art = (BoulderFahrtArt)(d & 0xFF);
  const uint8_t nr = (uint8_t)(d >> 8);
  if (s_dialog) {
    // Nie zwei zugleich: der Dauerteil wertet die ausbleibende Antwort als "keine".
    PBL_LOG_INFO("Boulder: Nachfrage %u uebergangen, ein Dialog steht noch", nr);
    return;
  }
  ActionableDialog *a = actionable_dialog_create("Boulder Fahrt");
  if (!a) {
    PBL_LOG_WRN("Boulder: Nachfrage %u ohne Speicher", nr);
    boulder_kontext_antwort(nr, NachfrageOhneAntwort);
    return;
  }
  s_dialog = a;
  s_nr = nr;
  s_antwort = NachfrageOhneAntwort;
  Dialog *dialog = actionable_dialog_get_dialog(a);
  actionable_dialog_set_action_bar_type(a, DialogActionBarConfirmDecline, NULL);
  actionable_dialog_set_click_config_provider(a, prv_tasten);
  dialog_set_text(dialog, (art == FahrtArtVelo) ? "Cycling?" : "Driving?");
  dialog_set_icon(dialog, RESOURCE_ID_WORKOUT_APP_DETECTED);
  dialog_set_background_color(dialog, PBL_IF_COLOR_ELSE(GColorYellow, GColorWhite));
  dialog_set_text_color(dialog, GColorBlack);
  // Am Lenker antwortet man oft nicht: nach 2 min schliesst er sich selbst (F4).
  dialog_set_timeout(dialog, KONTEXT_DIALOG_MIN * PBL_SEC_PER_MIN * 1000);
  dialog_set_callbacks(dialog, &(DialogCallbacks){ .unload = prv_zu }, NULL);
  actionable_dialog_push(a, modal_manager_get_window_stack(ModalPriorityGeneric));
  // In der Ruhezeit ohne Vibration; der Dialog erscheint trotzdem (3.4).
  if (!do_not_disturb_is_active()) {
    vibes_double_pulse();
  }
}

void boulder_nachfrage_stellen(BoulderFahrtArt art, uint8_t nr) {
  launcher_task_add_callback(prv_zeigen, (void *)(uintptr_t)(((uintptr_t)nr << 8) | art));
}
