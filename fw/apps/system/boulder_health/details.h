/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// Die Details der Karten (Spezifikation Schritt 3, 2.3), geoeffnet mit Select.
//
// Welche Karte welches Detail hat, steht in karten.h. Night, Heart und Sessions scrollen; Precision
// ist eine Auswahl der fuenf Messstufen. Gezeichnet wird wie die Karten nur aus BhDaten (L3).

#include "daten.h"
#include "karten.h"

#include "applib/graphics/gtypes.h"

// Hoehe des Inhalts unter dem Kopfbalken, fuer die ScrollLayer.
int16_t bh_detail_hoehe(BhDetail detail, const BhDaten *d);

// Zeichnet den Inhalt ab y = 0. `auswahl` ist die markierte Zeile in Precision (0..4).
void bh_detail_zeichnen(GContext *ctx, int16_t breite, BhDetail detail, const BhDaten *d,
                        uint8_t auswahl);
