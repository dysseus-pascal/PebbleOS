/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */
// Alle sichtbaren Texte der Health-App (Spezifikation Schritt 3, 2.5), eine Zeile je Text.
//
// KEIN #pragma once: texte.h und texte.c binden die Datei je einmal als X-Makro ein.
// Nur Englisch (Entscheid: Oberflaeche vorerst nur Englisch). Formate wie "7:12" oder "6,230"
// macht rechnung.c, hier stehen nur feste Woerter.
//
//   TEXT(schluessel, englisch)

// Titel der Karten (Reihenfolge wie BhKarte) und der Details
TEXT(BhTToday, "Today")
TEXT(BhTNight, "Night")
TEXT(BhTHeart, "Heart")
TEXT(BhTActivity, "Activity")
TEXT(BhTSettings, "Settings")
TEXT(BhTSessions, "Sessions")
TEXT(BhTPrecision, "Precision")

// Zeilen und Spalten
TEXT(BhTSleep, "Sleep")
TEXT(BhTResting, "Resting")
TEXT(BhTHrv, "HRV")
TEXT(BhTSteps, "Steps")
TEXT(BhTSpo2, "SpO2")
TEXT(BhTAwake, "Awake")
TEXT(BhTRem, "REM")
TEXT(BhTLight, "Light")
TEXT(BhTDeep, "Deep")
TEXT(BhTActive, "active")
TEXT(BhTAvg, "avg")
TEXT(BhTLowest, "lowest")

// Abschnitte der Details
TEXT(BhTDurchDieNacht, "THROUGH THE NIGHT")
TEXT(BhTKoerperImSchlaf, "BODY DURING SLEEP")
TEXT(BhTZonenHeute, "ZONES TODAY")
TEXT(BhTSpo224h, "SPO2 24 H")
TEXT(BhTHrv7, "HRV 7 NIGHTS")

// Zonen (Boulder: kiesel_gesundheit_strings.xml)
TEXT(BhTZoneLight, "Light")
TEXT(BhTZoneModerate, "Moderate")
TEXT(BhTZoneVigorous, "Vigorous")
TEXT(BhTZonePeak, "Peak")

// Urteile (Boulder: Tagesurteile.kt)
TEXT(BhTWellRested, "Well rested")
TEXT(BhTShortNight, "Short night")
TEXT(BhTVeryShortNight, "Very short night")
TEXT(BhTAsUsual, "as usual")
TEXT(BhTInRange, "in range")
TEXT(BhTAbove, "above")
TEXT(BhTBelow, "below")
TEXT(BhTSoFar, "so far")
TEXT(BhTNow, "now")

// Einheiten
TEXT(BhTBpm, "bpm")
TEXT(BhTMs, "ms")
TEXT(BhTMin, "min")
TEXT(BhTKcal, "kcal")
TEXT(BhTSchritteWort, "steps")

// Leer- und Sonderfaelle
TEXT(BhTTrackingOff, "Tracking off")
TEXT(BhTNoNight, "No night")
TEXT(BhTNoSessions, "No sessions today")
TEXT(BhTOff, "off")

// Arten der Sitzungen
TEXT(BhTWalk, "Walk")
TEXT(BhTRun, "Run")
TEXT(BhTRide, "Ride")
TEXT(BhTWorkout, "Workout")
TEXT(BhTNap, "Nap")
