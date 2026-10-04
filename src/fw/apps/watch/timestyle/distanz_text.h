/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// Boulder: der Text des Widgets "Steps" mit der Einstellung Distanz -
// herausgeloest aus StepCounter_draw (sidebar_widgets.c), damit ihn ein
// Host-Test pruefen kann (dieselben Testvektoren wie Boulders TsTexteTest).
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Metrisch: "57m" unter 100 m, ".4km" unter 1 km (Zehntel, mit [trenner]),
// sonst ganze km ("4km"). Sonst Meilen: gerundet ganze Meilen ("3mi"), unter
// einer halben Meile die Zehntel (".4mi"). Rueckgabe: ob die kleine Schrift
// gilt (ab 10 km, wie im Original).
bool distanz_text(char *puffer, size_t groesse, int32_t meter, bool metrisch, char trenner);
