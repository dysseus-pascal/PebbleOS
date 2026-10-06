/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// Boulder: der Kernel-Heap fuer Tests, die diagnose.c mitbauen - ein echter Heap
// (lib/util/heap.c) ueber einem Feld, damit der Niedrigstand wie auf der Uhr entsteht.

#include "kernel/kernel_heap.h"

#include <stdbool.h>
#include <stdint.h>

static uint8_t s_feld[16 * 1024] __attribute__((aligned(8)));
static Heap s_heap;
static bool s_bereit;

Heap *kernel_heap_get(void) {
  if (!s_bereit) {
    heap_init(&s_heap, s_feld, s_feld + sizeof(s_feld), false);
    s_bereit = true;
  }
  return &s_heap;
}
