/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// Boulder: schickt jetzt, was die Uhr fuer das Telefon gesammelt hat - die
// Minuten bis eben und alle Datalogging-Sitzungen, wie auf eine Anfrage des
// Telefons (Endpunkt 911), aber ohne Antwort.
//
// WARUM: sonst schiebt die Uhr nur im 15-Minuten-Takt von Datalogging. Ein
// eben gespeichertes Training oder eine eben fertige Nacht soll gleich auf
// dem Telefon sein, nicht bis zu einer Viertelstunde spaeter. Der Takt selbst
// bleibt, wie er ist.
//
// Die Arbeit laeuft auf KernelBG; aufrufen darf man von jeder Aufgabe aus.
void health_sync_schicken(void);
