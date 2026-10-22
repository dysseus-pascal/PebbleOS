/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once
#include "plattform.h"

// Der Rhythmus der Atemuebung (AtemWert, kieselsport.h), wie das Telefon ihn eingestellt hat.
//
// DER DIENST HAELT IHN, NICHT DIE APP: Er liegt im Persist neben den anderen Einstellungen, und die
// Grenzen gelten hier fuer beides, fuer das Telefon und fuer das, was im Persist steht. Gezaehlt
// wird die Uebung in der App (lauf_window.c); der Dienst misst nur die Zeit.

// Aus dem Persist; was dort fehlt oder ausserhalb der Grenzen liegt, behaelt seinen Wert.
void rhythmus_laden(void);
Atemrhythmus rhythmus(void);
// Ausserhalb der Grenzen oder unveraendert: nichts, und nichts in den Flash.
void rhythmus_setze(AtemWert welcher, int32_t wert);

#if UNITTEST
// Ein Neustart der Uhr (kieselsport_test_neustart): im RAM steht wieder die Vorgabe.
void rhythmus_test_neustart(void);
#endif
