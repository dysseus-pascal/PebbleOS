/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// Boulder: die Schrittzahl der Leiste, wie TimeStyle sie schreibt - aus
// StepCounter_draw (sidebar_widgets.c) herausgeloest, damit ein Host-Test sie
// pruefen kann.
#pragma once

#include <stddef.h>
#include <stdint.h>

// Unter 1000 die ganze Zahl ("734"), bis 9999 Tausender mit einer Stelle
// ("5.5k", abgeschnitten wie im Original: 5590 bleibt 5.5k), darueber nur
// Tausender ("12k"). trenner ist das Dezimalzeichen der Einstellungen.
// Negative Werte (keine Daten) zaehlen als 0.
void schritte_text(char *puffer, size_t groesse, int32_t schritte, char trenner);
