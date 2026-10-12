// Probe: die Reihenfolge von prv_detail_entladen (boulder_health.c:186-199) mit Pebbles echter
// layer.c und scroll_layer.c, unter AddressSanitizer.
#include "applib/ui/scroll_layer.h"
#include "applib/ui/layer.h"
#include "clar.h"

#include "stubs_app_state.h"
#include "stubs_compiled_with_legacy2_sdk.h"
#include "stubs_content_indicator.h"
#include "stubs_graphics_context.h"
#include "stubs_heap.h"
#include "stubs_logging.h"
#include "stubs_passert.h"
#include "stubs_pbl_malloc.h"
#include "stubs_pebble_tasks.h"
#include "stubs_resources.h"
#include "stubs_syscalls.h"
#include "stubs_unobstructed_area.h"

#include <stdio.h>

void graphics_draw_bitmap_in_rect(GContext *ctx, const GBitmap *src_bitmap, const GRect *rect) {}
bool graphics_release_frame_buffer(GContext *ctx, GBitmap *buffer) { return false; }
void window_schedule_render(struct Window *window) {}
void window_set_click_config_provider_with_context(struct Window *window,
    ClickConfigProvider click_config_provider, void *context) {}
void window_set_click_context(ButtonId button_id, void *context) {}
void window_single_repeating_click_subscribe(ButtonId button_id, uint16_t repeat_interval_ms,
                                             ClickHandler handler) {}

int main(int argc, char **argv) {
  const bool app_reihenfolge = (argc < 2);
  // Wurzel des Detailfensters (im echten Fenster eingebettet)
  Layer wurzel;
  layer_init(&wurzel, &GRect(0, 0, 200, 228));
  // prv_detail_oeffnen: Kopf, Inhalt in der ScrollLayer, ScrollLayer an der Wurzel
  Layer *kopf = layer_create(GRect(0, 0, 200, 28));
  Layer *inhalt = layer_create(GRect(0, 0, 200, 332));
  ScrollLayer *rolle = scroll_layer_create(GRect(0, 28, 200, 200));
  scroll_layer_set_content_size(rolle, GSize(200, 332));
  scroll_layer_add_child(rolle, inhalt);
  layer_add_child(&wurzel, scroll_layer_get_layer(rolle));
  layer_add_child(&wurzel, kopf);
  if (app_reihenfolge) {
    // prv_detail_entladen
    printf("Reihenfolge der App: scroll_layer_destroy, dann layer_destroy(inhalt)\n");
    scroll_layer_destroy(rolle);
    layer_destroy(inhalt);
    layer_destroy(kopf);
  } else {
    printf("Gegenprobe: erst das Kind, dann die ScrollLayer\n");
    layer_destroy(inhalt);
    scroll_layer_destroy(rolle);
    layer_destroy(kopf);
  }
  printf("fertig\n");
  return 0;
}
