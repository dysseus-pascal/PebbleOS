/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */
// Alle festen Texte der Navigation, eine Zeile je Text.
//
// ACHTUNG, ZWEI DINGE SIND ABSICHT (wie bei den Schwesterapps):
//  1. KEIN #pragma once und keine Include-Waechter. Diese Datei wird MEHRFACH
//     eingebunden (X-Makro): einmal fuer die Aufzaehlung der Schluessel in
//     texte.h und einmal fuer die Tabelle in texte.c. Ein Waechter wuerde
//     die zweite Einbindung verschlucken und eine leere Tabelle erzeugen.
//  2. Endung .h, obwohl es kein gewoehnlicher Header ist: Build-Umgebungen,
//     die nur .c und .h in ihren Baum kopieren, finden sonst nichts.
//
//   STR(schluessel, maxbytes, en, de, fr, it, es)
//
//   maxbytes  Groesse des Zielpuffers in BYTES, NACH dem Einsetzen der Werte
//             (ein "%s" oder "%d" also so lang gerechnet, wie es wird);
//             0, wenn der Text in keinen festen Puffer kopiert wird.
//             Akzente und Umlaute zaehlen als zwei Bytes, "·" ebenso.
//   en        Englisch. Spalte 0 und zugleich der Rueckfall.
//
// Fehlt eine Spalte in auch nur einer Zeile, ist das ein Praeprozessorfehler -
// kein stiller Rueckfall.
//
// Franzoesisch, Italienisch und Spanisch sind meist laenger. Lieber knapp
// uebersetzen als am Steuer eine abgeschnittene Zeile lesen.

// ---- Kein gueltiger Stand -------------------------------------------------
// Gross, bis zu zwei Zeilen.
STR(STR_KEINE_NAVIGATION, 64, "No navigation", "Keine Navigation",
    "Pas de navigation", "Nessuna navigazione", "Sin navegación")

// Klein darunter: wann zuletzt etwas kam. Puffer 40 Byte, das %s ist einer
// der Alterstexte unten (hoechstens 27 Byte).
STR(STR_ZULETZT,          40, "updated %s", "zuletzt %s",
    "mis à jour %s", "aggiornato %s", "actualizado %s")

// Alterstexte, Puffer 28 Byte.
STR(STR_ALTER_NIE,        28, "never", "noch nie",
    "jamais", "mai", "nunca")
STR(STR_ALTER_JETZT,      28, "just now", "gerade eben",
    "à l'instant", "proprio ora", "ahora mismo")
STR(STR_ALTER_MIN,        28, "%d min ago", "vor %d min",
    "il y a %d min", "%d min fa", "hace %d min")
STR(STR_ALTER_STD,        28, "%d h ago", "vor %d Std",
    "il y a %d h", "%d h fa", "hace %d h")
STR(STR_ALTER_GESTERN,    28, "yesterday or earlier", "gestern oder früher",
    "hier ou avant", "ieri o prima", "ayer o antes")

// ---- Fusszeile: Ankunftszeit ----------------------------------------------
// Das %s ist die Uhrzeit (5 Byte). Puffer 40 Byte, davor steht hoechstens
// "999999 km · " - also knapp halten, die Zeile ist EINE Zeile und wird sonst
// mit Auslassungspunkten abgeschnitten.
STR(STR_ANKUNFT,          40, "ETA %s", "an %s",
    "arr. %s", "arr. %s", "lleg. %s")

// ---- Entfernung ----------------------------------------------------------
// Dezimalzeichen in "1,2 km". LECO hat beide.
STR(STR_DEZIMAL,           0, ".", ",", ",", ",", ",")

// Die Zeile im Starter (App Glance) der Store-App ist weg: die System-App
// zeigt im Startmenue nur ihren Namen.
