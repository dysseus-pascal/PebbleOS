/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// Boulder: die Stunde des Widgets "Time zone" - aus SidebarWidgets_updateTime
// (sidebar_widgets.c) herausgeloest, damit ein Host-Test sie pruefen kann.
#pragma once

#include <stdbool.h>
#include <stddef.h>

// Die Stunde in der Zone UTC+versatz_h zur Ortszeit stunde:minute, deren
// Abstand zu UTC gmtoff_s ist (tm_gmtoff, MIT Sommerzeit). 12 h: "8a"/"12p",
// 24 h: "8"/"20"; mit fuehrender Null "08a" bzw. "08".
void zweitzeit_text(char *puffer, size_t groesse, int stunde, int minute, long gmtoff_s,
                    int versatz_h, bool h24, bool fuehrende_null);
