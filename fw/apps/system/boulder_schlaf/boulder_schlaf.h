/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// Boulder: der Schlafmodus (F53) als Schalter fuer Quick Launch, wie Pebbles Schalter in
// fw/apps/system/toggle/. Nur mit der Variante; die Registry fuehrt ihn als BOULDER_SCHLAF (-105).

// Aus dem Kern (S8 der Health-App): den Schalter oeffnen wie Quick Launch.
void boulder_schlaf_oeffnen(void);
