/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// Boulder: zwei Zeilen von Pebbles Settings > Health (health.c) nach Boulders Regeln; health.c ruft
// sie nur mit der Variante. Die Uhr stellt Puls- und SpO2-Takt selbst nach der Messstufe (F3),
// darum zeigt die Zeile "HR Monitoring" die Messstufe und die Zeile "SpO2 Monitoring" den Schalter
// SpO2 im Schlaf (Entscheid vom 22.10.2026).

#pragma once

void boulder_health_messstufe_zeile(const char **title, const char **subtitle);
// Oeffnet die Auswahl der Stufe; [context] ist die Seite (SettingsHealthData), die die Texte haelt.
void boulder_health_messstufe_waehlen(void *context);

void boulder_health_spo2_schlaf_zeile(const char **title, const char **subtitle);
void boulder_health_spo2_schlaf_waehlen(void);
