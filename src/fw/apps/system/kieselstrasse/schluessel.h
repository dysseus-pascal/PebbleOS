/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// Die Schluessel der Nachrichten vom Telefon - DIESELBEN ZAHLEN WIE IN DER
// STORE-APP. Dort vergab sie das SDK der Reihe nach ab 10000, in der
// Reihenfolge von messageKeys in der package.json; Boulder schickt sie fest
// mit diesen Zahlen (Kieselstrasse.kt). Wer hier eine Zahl aendert, spricht
// mit dem Telefon in einer anderen Sprache.
#define MESSAGE_KEY_ABBIEGEART 10000
#define MESSAGE_KEY_ENTFERNUNG 10001
#define MESSAGE_KEY_STRASSE 10002
#define MESSAGE_KEY_ANKUNFT 10003
#define MESSAGE_KEY_REST 10004

// Der letzte Stand im Persist. Dieselbe Nummer wie in der Store-App: die
// System-App traegt deren Kennung und liest so, was jene zuletzt schrieb.
#define PERSIST_WEG 1
