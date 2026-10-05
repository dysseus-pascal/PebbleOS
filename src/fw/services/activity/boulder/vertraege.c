/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// DIE VERTRAEGE, DIE BOULDER MIT UPSTREAM TEILT - beim Bauen bewacht.
//
// Boulder liest Pebbles Sitzungen (Tag 84, Fassung 7 mit `manual`) und legt die
// erkannte Bewegung als eigenen Typ 8 hinein; die Nacht geht auf eigenen
// Datalogging-Tags. Aendert ein Upstream-Merge einen dieser Werte oder vergibt
// einen neuen Typ oder Tag, soll der Bau brechen - nicht erst die App, die
// Daten stillschweigend falsch liest oder quittiert und verwirft.
//
// Was der Compiler hier nicht sieht - ein neuer Name mit einem schon
// vergebenen Wert, etwa `Swim = 8` vor `Movement = 8` -, prueft
// tools/boulder/vertraege.py gegen eine eingefrorene Liste (Bauschritt und
// ctest). Wer hier etwas aendert, aendert die Liste dort mit.
//
// Ohne Code fuer die Uhr: die Funktionen ruft niemand, der Linker wirft sie weg.

#include "pbl/services/activity/activity.h"
#include "pbl/services/activity/activity_private.h"
#include "pbl/services/activity/boulder/nacht.h"
#include "pbl/services/data_logging/data_logging_service.h"

#include <stddef.h>
#include <stdint.h>

// Tag 84: Boulder liest Fassung 7. Die Fassung ist ein Bitfeld - Bit 0 "lesbar",
// Bit 1 "mit Schrittdaten" (gilt auch fuer Tag 83), Bit 2 "mit manual".
_Static_assert(ACTIVITY_SESSION_LOGGING_VERSION == 7, "Tag 84: Boulder liest Fassung 7");
_Static_assert((ACTIVITY_SESSION_LOGGING_VERSION & 0x3) == 0x3, "Tag 83/84: Bit 0 und Bit 1");
_Static_assert((ACTIVITY_SESSION_LOGGING_VERSION & 0x4) != 0, "Tag 84: Bit 2 = manual");
_Static_assert(offsetof(ActivitySessionDataLoggingRecord, manual) == 26, "manual an Byte 26");
_Static_assert(sizeof(ActivitySessionDataLoggingRecord) == 27, "Tag 84: Satz 27 Byte");

// Typ 8 ist Boulders Bewegung; Pebble kennt 0 bis 7.
_Static_assert(ActivitySessionType_Movement == 8, "Bewegung ist Typ 8");
_Static_assert(ActivitySessionTypeCount == 9, "Sitzungstypen 0 bis 8");

// Boulders Tags liegen weit hinter Pebbles (78 an aufwaerts).
_Static_assert(BOULDER_DLS_TAG_NACHT >= 0x4000u && BOULDER_DLS_TAG_NACHT_HRV >= 0x4000u,
               "Boulder-Tags ab 0x4000");

int boulder_vertrag_typen(ActivitySessionType typ);
int boulder_vertrag_tags(DlsSystemTag tag);
int boulder_vertrag_tags_vergeben(uint32_t tag);

// Ohne default: jeder neue Upstream-Typ bricht hier den Bau (-Wswitch, -Werror).
int boulder_vertrag_typen(ActivitySessionType typ) {
  switch (typ) {
    case ActivitySessionType_None:
    case ActivitySessionType_Sleep:
    case ActivitySessionType_RestfulSleep:
    case ActivitySessionType_Nap:
    case ActivitySessionType_RestfulNap:
    case ActivitySessionType_Walk:
    case ActivitySessionType_Run:
    case ActivitySessionType_Open:
    case ActivitySessionType_Movement:
    case ActivitySessionTypeCount:
      return (int)typ;
  }
  return -1;
}

// Ohne default: jeder neue Upstream-Tag bricht hier den Bau. Dann gehoert er
// auch in die Liste darunter.
int boulder_vertrag_tags(DlsSystemTag tag) {
  switch (tag) {
    case DlsSystemTagAnalyticsDeviceHeartbeat:
    case DlsSystemTagAnalyticsAppHeartbeat:
    case DlsSystemTagAnalyticsEvent:
    case DlsSystemTagActivityMinuteData:
    case DlsSystemTagActivityAccelSamples:
    case DlsSystemTagActivitySession:
    case DlsSystemTagProtobufLogSession:
    case DlsSystemTagAnalyticsNativeHeartbeat:
      return (int)tag;
  }
  return -1;
}

// ALLE VERGEBENEN SYSTEM-TAGS ALS ZAHLEN: ein doppelter case-Wert bricht den Bau.
// Vergibt Upstream einen Tag, den Boulder hat oder hatte, faellt es hier auf.
int boulder_vertrag_tags_vergeben(uint32_t tag) {
  switch (tag) {
    case DlsSystemTagAnalyticsDeviceHeartbeat:
    case DlsSystemTagAnalyticsAppHeartbeat:
    case DlsSystemTagAnalyticsEvent:
    case DlsSystemTagActivityMinuteData:
    case DlsSystemTagActivityAccelSamples:
    case 83:  // Schlaf alter Firmware; Boulder liest ihn wie 84
    case DlsSystemTagActivitySession:
    case DlsSystemTagProtobufLogSession:
    case 86:  // bei Pebble ausser Dienst, nie wieder vergeben
    case DlsSystemTagAnalyticsNativeHeartbeat:
    case BOULDER_DLS_TAG_NACHT_ALT:
    case BOULDER_DLS_TAG_NACHT_HRV_ALT:
    case BOULDER_DLS_TAG_NACHT:
    case BOULDER_DLS_TAG_NACHT_HRV:
      return 1;
  }
  return 0;
}
