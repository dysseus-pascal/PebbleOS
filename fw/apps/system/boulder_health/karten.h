/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// Die fuenf Karten der Health-App (Spezifikation Schritt 3, 2.1-2.2) in ihrer Reihenfolge.
//
// Hier steht der Rahmen: der Kopfbalken mit Titel, Farbe und den fuenf Punkten. Den Inhalt zeichnet
// inhalt.c, die Details details.c.

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

// Die Farbe des Kopfbalkens (2.1, Entscheid F14).
GColor bh_karte_farbe(BhKarte karte);

// Der Kopfbalken (2.1): 0-27 px, Titel in GOTHIC 18 fett und weiss, rechts ein Punkt je Karte,
// der der gezeigten Karte gefuellt.
void bh_kopf_zeichnen(GContext *ctx, GRect flaeche, BhKarte karte);

// Das Detail einer Karte (2.3), geoeffnet mit Select. Today hat keins (Entscheid F12).
typedef enum {
  BhDetailKeins,
  BhDetailNight,
  BhDetailHeart,
  BhDetailSessions,
  BhDetailPrecision,
} BhDetail;

BhDetail bh_detail_von(BhKarte karte);
// Titel im Kopfbalken des Details.
const char *bh_detail_titel(BhDetail detail);
