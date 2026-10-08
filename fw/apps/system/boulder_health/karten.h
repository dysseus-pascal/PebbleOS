/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// Die fuenf Karten der Health-App (Spezifikation Schritt 3, 2.1-2.2) in ihrer Reihenfolge.
//
// Noch nur ihr Rahmen (AP7): der Kopfbalken mit Titel und den fuenf Punkten. So zeigt jedes Bild
// aus QEMU, auf welcher Karte ein Einstieg landet. Inhalt, Farbe je Karte und Details kommen mit
// AP6.

#include "applib/graphics/gtypes.h"

typedef enum {
  BhKarteToday,
  BhKarteNight,
  BhKarteHeart,
  BhKarteActivity,
  BhKarteSettings,
  BhKartenAnzahl,
} BhKarte;

// Der Titel im Kopfbalken (2.5, Gruppe "Titel").
const char *bh_karte_titel(BhKarte karte);

// Der Kopfbalken (2.1): 0-27 px, Titel in GOTHIC 18 fett und weiss, rechts ein Punkt je Karte,
// der der gezeigten Karte gefuellt.
void bh_kopf_zeichnen(GContext *ctx, GRect flaeche, BhKarte karte);
