/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// Boulder: P6, die Kachel von Kieselsport im Startmenue (app_glance_kieselsport.c). Mit der
// Kennung der App, damit app_glance_service.c nur diesen einen Kopf braucht.

#pragma once

#include "app_glance.h"

#include "process_management/app_menu_data_source.h"
#include "pbl/services/activity/boulder/sport/kieselsport.h"

LauncherAppGlance *launcher_app_glance_kieselsport_create(const AppMenuNode *node);
