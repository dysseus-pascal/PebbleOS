/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// DIE VERTRAEGE, DIE BOULDER MIT UPSTREAM TEILT - beim Bauen bewacht (aus b26,
// fuer V2: Pebbles Sitzungstypen 0 bis 7, kein eigener Typ; Tag 84 in Pebbles
// Fassung 3). Aendert ein Upstream-Merge einen Wert oder vergibt einen neuen Typ
// oder Tag, bricht der Bau - nicht erst die App, die still falsch liest.
// Was der Compiler nicht sieht (ein neuer Name auf einem vergebenen Wert),
// prueft tools/boulder/vertraege.py (ctest). Die Funktionen ruft niemand.

#include "pbl/services/activity/activity.h"
#include "pbl/services/activity/activity_private.h"
#include "pbl/services/activity/boulder/nacht_satz.h"
#include "pbl/services/data_logging/data_logging_service.h"

#include <stddef.h>
#include <stdint.h>

// Tag 84: Fassung 3, Satz 26 Byte (Schrittdaten im Anhang).
_Static_assert(ACTIVITY_SESSION_LOGGING_VERSION == 3, "Tag 84: Fassung 3");
_Static_assert(sizeof(ActivitySessionDataLoggingRecord) == 26, "Tag 84: Satz 26 Byte");
_Static_assert(offsetof(ActivitySessionDataLoggingRecord, start_utc) == 10, "Tag 84: start_utc");
_Static_assert(offsetof(ActivitySessionDataLoggingRecord, step_data) == 18, "Tag 84: Anhang");
_Static_assert(ActivitySessionTypeCount == 8, "Sitzungstypen 0 bis 7, kein eigener Typ");
_Static_assert(BOULDER_DLS_TAG_NACHT == 0x4001u && BOULDER_DLS_TAG_NACHT_HRV == 0x4002u,
               "Boulder-Tags 0x4001/0x4002");

int boulder_vertrag_typen(ActivitySessionType typ);
int boulder_vertrag_tags(DlsSystemTag tag);
int boulder_vertrag_tags_vergeben(uint32_t tag);

// Ohne default: jeder neue Upstream-Typ bricht hier den Bau (-Wswitch).
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
    case ActivitySessionTypeCount:
      return (int)typ;
  }
  return -1;
}

// Ohne default: jeder neue Upstream-Tag bricht hier den Bau.
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

// Boulders Tags und die gesperrten liegen nicht auf einem Upstream-Tag: zwei
// gleiche case-Werte brechen den Bau.
int boulder_vertrag_tags_vergeben(uint32_t tag) {
  switch (tag) {
    case DlsSystemTagAnalyticsDeviceHeartbeat:
    case DlsSystemTagAnalyticsAppHeartbeat:
    case DlsSystemTagAnalyticsEvent:
    case DlsSystemTagActivityMinuteData:
    case DlsSystemTagActivityAccelSamples:
    case DlsSystemTagActivitySession:
    case DlsSystemTagProtobufLogSession:
    case DlsSystemTagAnalyticsNativeHeartbeat:
    case 83:  // alter Schlaf, die App liest ihn wie 84
    case 86:  // bei Pebble ausser Dienst
    case BOULDER_DLS_TAG_NACHT_ALT:
    case BOULDER_DLS_TAG_NACHT_HRV_ALT:
    case BOULDER_DLS_TAG_NACHT:
    case BOULDER_DLS_TAG_NACHT_HRV:
      return 1;
  }
  return 0;
}
