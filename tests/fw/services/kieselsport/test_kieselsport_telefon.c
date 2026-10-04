/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "stubs_logging.h"
#include "stubs_pbl_malloc.h"

// Zuletzt: telefon.h zieht pebble_compat.h nach, und dessen Makros
// (malloc, time, ...) sollen die Koepfe davor nicht umschreiben.
#include "apps/system/kieselsport/einstellungen.h"
#include "apps/system/kieselsport/meldungen.h"
#include "apps/system/kieselsport/schluessel.h"
#include "apps/system/kieselsport/telefon.h"
#undef malloc
#undef calloc
#undef free
#undef time

// Der Draht der App zum Telefon (telefon.c): wer wann in den Postausgang
// darf. Nachgebaut sind der Postausgang (fasst genau eine Nachricht, bis das
// Telefon antwortet), die Wecker mit einer Uhr in Millisekunden und der
// Persist des Dienstes - Zusammenfassung, Stopp und Kurve.

#define BEGINN 1790445600u
#define KS_ART 1

// --- Uhrzeit ---

static time_t s_jetzt;
time_t rtc_get_time(void) { return s_jetzt; }

// --- Wecker ---

typedef struct {
  uint32_t id;           //< 0 = frei
  uint32_t faellig_ms;
  AppTimerCallback cb;
  void *daten;
} Wecker;

#define WECKER_MAX 16
static Wecker s_wecker[WECKER_MAX];
static uint32_t s_wecker_id;
static uint32_t s_ms;

AppTimer *app_timer_register(uint32_t timeout_ms, AppTimerCallback cb, void *daten) {
  for (int i = 0; i < WECKER_MAX; i++) {
    if (!s_wecker[i].id) {
      s_wecker[i] = (Wecker){ .id = ++s_wecker_id, .faellig_ms = s_ms + timeout_ms,
                              .cb = cb, .daten = daten };
      return (AppTimer *)(uintptr_t)s_wecker[i].id;
    }
  }
  cl_fail("zu viele Wecker");
  return NULL;
}

void app_timer_cancel(AppTimer *timer) {
  for (int i = 0; i < WECKER_MAX; i++) {
    if (s_wecker[i].id && s_wecker[i].id == (uint32_t)(uintptr_t)timer) {
      s_wecker[i].id = 0;
    }
  }
}

// Laesst [ms] vergehen; was faellig wird, laeuft in der Reihenfolge der Zeit.
static void prv_warte(uint32_t ms) {
  const uint32_t bis = s_ms + ms;
  for (;;) {
    int n = -1;
    for (int i = 0; i < WECKER_MAX; i++) {
      if (s_wecker[i].id && s_wecker[i].faellig_ms <= bis &&
          (n < 0 || s_wecker[i].faellig_ms < s_wecker[n].faellig_ms)) {
        n = i;
      }
    }
    if (n < 0) break;
    const Wecker w = s_wecker[n];
    s_wecker[n].id = 0;
    if (w.faellig_ms > s_ms) s_ms = w.faellig_ms;
    w.cb(w.daten);
  }
  s_ms = bis;
}

// --- Verbindung ---

// Ob das Telefon verbunden ist - nur gelesen, wie connection_service es tut.
static bool s_verbunden;
bool connection_service_peek_pebble_app_connection(void) { return s_verbunden; }

// --- Postausgang ---
//
// WIE app_message_outbox.c: geschlossen, bereit, wird geschrieben (nach
// begin, vor send) und wartet auf das Telefon. Ein zweites begin ohne send
// dazwischen scheitert mit INVALID_STATE - genau wie auf der Uhr.

#define NACHRICHT_MAX 700
typedef struct {
  uint8_t daten[NACHRICHT_MAX];
  uint16_t laenge;
} Nachricht;

#define GESENDET_MAX 48
static Nachricht s_gesendet[GESENDET_MAX];
static int s_anzahl;
static uint8_t s_out[NACHRICHT_MAX];
static DictionaryIterator s_out_iter;
static bool s_belegt;   //< eine Nachricht wartet auf die Antwort des Telefons
static bool s_schreibt; //< begin ohne send: OUT_WRITING
static bool s_offen;    //< app_message_open gelang
static AppMessageResult s_open_ergebnis;
static int s_begin_versuche;

static AppMessageInboxReceived s_bei_eingang;
static AppMessageOutboxSent s_bei_angekommen;
static AppMessageOutboxFailed s_bei_abgelehnt;

AppMessageInboxReceived app_message_register_inbox_received(AppMessageInboxReceived cb) {
  s_bei_eingang = cb;
  return NULL;
}
AppMessageOutboxSent app_message_register_outbox_sent(AppMessageOutboxSent cb) {
  s_bei_angekommen = cb;
  return NULL;
}
AppMessageOutboxFailed app_message_register_outbox_failed(AppMessageOutboxFailed cb) {
  s_bei_abgelehnt = cb;
  return NULL;
}
AppMessageResult app_message_open(const uint32_t ein, const uint32_t aus) {
  cl_assert(aus <= NACHRICHT_MAX);
  s_offen = s_open_ergebnis == APP_MSG_OK;
  return s_open_ergebnis;
}

AppMessageResult app_message_outbox_begin(DictionaryIterator **iter) {
  s_begin_versuche++;
  *iter = NULL;
  if (s_belegt) return APP_MSG_BUSY;
  if (s_schreibt || !s_offen) return APP_MSG_INVALID_STATE;
  dict_write_begin(&s_out_iter, s_out, sizeof(s_out));
  *iter = &s_out_iter;
  s_schreibt = true;
  return APP_MSG_OK;
}

AppMessageResult app_message_outbox_send(void) {
  if (s_belegt) return APP_MSG_BUSY;
  if (!s_schreibt) return APP_MSG_INVALID_STATE;
  s_schreibt = false;
  const uint32_t n = dict_write_end(&s_out_iter);
  cl_assert(s_anzahl < GESENDET_MAX);
  memcpy(s_gesendet[s_anzahl].daten, s_out, n);
  s_gesendet[s_anzahl].laenge = (uint16_t)n;
  s_anzahl++;
  s_belegt = true;
  return APP_MSG_OK;
}

// Das Telefon antwortet auf die Nachricht im Postausgang.
static void prv_antwort(bool angekommen) {
  cl_assert(s_belegt);
  s_belegt = false;
  if (angekommen) {
    s_bei_angekommen(&s_out_iter, NULL);
  } else {
    s_bei_abgelehnt(&s_out_iter, APP_MSG_SEND_TIMEOUT, NULL);
  }
}

// Der Wert unter [key] in Nachricht [i], -1 wenn er fehlt.
static int32_t prv_feld(int i, uint32_t key) {
  cl_assert(i < s_anzahl);
  DictionaryIterator it;
  dict_read_begin_from_buffer(&it, s_gesendet[i].daten, s_gesendet[i].laenge);
  Tuple *t = dict_find(&it, key);
  return t ? t->value->int32 : -1;
}

static bool prv_hat(int i, uint32_t key) {
  DictionaryIterator it;
  dict_read_begin_from_buffer(&it, s_gesendet[i].daten, s_gesendet[i].laenge);
  return dict_find(&it, key) != NULL;
}

typedef enum { ArtZustand, ArtZusammenfassung, ArtKurve, ArtEinst } Nachrichtenart;

static Nachrichtenart prv_art(int i) {
  if (prv_hat(i, MESSAGE_KEY_DAUER)) return ArtZusammenfassung;
  if (prv_hat(i, MESSAGE_KEY_KURVE)) return ArtKurve;
  if (prv_hat(i, MESSAGE_KEY_MAXPULS)) return ArtEinst;
  return ArtZustand;
}

// --- Der Dienst (Persist) ---

static bool s_wartet;
static Trainingsstand s_stand;
static const uint8_t s_pausen[] = { KS_PAUSEN_FASSUNG, 1, 30, 0, 120, 0 };

bool kieselsport_wartend_laden(Trainingsstand *t, char *liste, size_t platz) {
  if (!s_wartet) return false;
  *t = s_stand;
  liste[0] = 0;
  return true;
}
void kieselsport_wartend_vergessen(uint32_t beginn) {
  if (beginn == 0 || beginn == s_stand.beginn) s_wartet = false;
}
size_t kieselsport_wartend_pausen(uint8_t *aus, size_t platz) {
  memcpy(aus, s_pausen, sizeof(s_pausen));
  return sizeof(s_pausen);
}
uint32_t kieselsport_wartend_ende(void) { return BEGINN + 300; }

static bool s_stopp;
static uint32_t s_stopp_beginn;
static uint32_t s_stopp_zeit;
bool kieselsport_stopp_laden(uint32_t *beginn, uint32_t *zeit, uint8_t *art) {
  if (!s_stopp) return false;
  *beginn = s_stopp_beginn;
  *zeit = s_stopp_zeit;
  *art = KS_ART;
  return true;
}
void kieselsport_stopp_vergessen(uint32_t beginn) {
  if (beginn == 0 || beginn == s_stopp_beginn) s_stopp = false;
}

static uint16_t s_kurve_anzahl;
static uint16_t s_kurve_ab;
static uint32_t s_kurve_beginn;
static int s_kurve_leer;      //< so oft liefert das Lesen nichts
static bool s_kurve_luecke;   //< der Schluessel des naechsten Stuecks fehlt
bool kieselsport_kurve_wartet(void) { return s_kurve_ab < s_kurve_anzahl; }
uint32_t kieselsport_kurve_beginn(void) { return s_kurve_beginn; }
uint16_t kieselsport_kurve_anzahl(void) { return s_kurve_anzahl; }
uint16_t kieselsport_kurve_ab(void) { return s_kurve_ab; }
uint16_t kieselsport_kurve_stueck(uint8_t *aus, uint16_t max) {
  if (s_kurve_leer > 0) {
    s_kurve_leer--;
    return 0;
  }
  const uint16_t rest = (uint16_t)(s_kurve_anzahl - s_kurve_ab);
  const uint16_t n = rest < max ? rest : max;
  memset(aus, 120, n);
  return n;
}
// Wie kurve.c: nur das Stueck, das hinausging, rueckt den Zeiger vor.
bool kieselsport_kurve_bestaetigt(uint32_t beginn, uint16_t ab, uint16_t n) {
  if (beginn != s_kurve_beginn || ab != s_kurve_ab || s_kurve_ab >= s_kurve_anzahl) return false;
  s_kurve_ab = (uint16_t)(s_kurve_ab + n);
  return true;
}
bool kieselsport_kurve_luecke(void) { return s_kurve_luecke; }
void kieselsport_kurve_vergessen(void) { s_kurve_anzahl = s_kurve_ab = 0; }

// --- Einstellungen ---

static int32_t s_alt_zahl;
static int s_vermerkt;   //< -1 = nicht gerufen
static int32_t s_vermerkt_zahl;
void einstellungen_maxpuls(int32_t schlaege) { s_alt_zahl = schlaege; }
void einstellungen_maxpuls_vermerkt(bool gesetzt, int32_t schlaege) {
  s_vermerkt = gesetzt ? 1 : 0;
  s_vermerkt_zahl = schlaege;
}
void einstellungen_becken(int32_t meter) {}
void einstellungen_pausenziel(int32_t sekunden) {}
void einstellungen_empfindlichkeit(int32_t stufe) {}
void einstellungen_pin_art(int32_t art) {}
void einstellungen_pin_zeit(const char *hhmm) {}
void einstellungen_melden(DictionaryIterator *out) {
  dict_write_int32(out, MESSAGE_KEY_MAXPULS, 179);
  dict_write_int32(out, MESSAGE_KEY_MAXPULS_GESETZT, 0);
}

static int s_fertig;
static void prv_fertig(void) { s_fertig++; }

// --- Hilfen ---

static void prv_zusammenfassung_wartet(uint32_t beginn) {
  s_wartet = true;
  s_stand = (Trainingsstand){ .art = KS_ART, .beginn = beginn, .dauer_s = 240 };
}

static bool prv_wecker_offen(void) {
  for (int i = 0; i < WECKER_MAX; i++) {
    if (s_wecker[i].id) return true;
  }
  return false;
}

// Bestaetigt alles, was hinausgeht, bis nichts mehr ansteht.
static void prv_alles_bestaetigen(void) {
  for (int runde = 0; runde < 200; runde++) {
    if (s_belegt) {
      prv_antwort(true);
    } else if (!prv_wecker_offen()) {
      return;
    }
    prv_warte(500);
  }
  cl_fail("es hoert nicht auf zu senden");
}

void test_kieselsport_telefon__initialize(void) {
  memset(s_wecker, 0, sizeof(s_wecker));
  s_wecker_id = 0;
  s_ms = 0;
  s_jetzt = BEGINN;
  s_anzahl = 0;
  s_belegt = false;
  s_schreibt = false;
  s_offen = false;
  s_open_ergebnis = APP_MSG_OK;
  s_begin_versuche = 0;
  s_verbunden = true;
  s_wartet = false;
  s_stopp = false;
  s_kurve_anzahl = s_kurve_ab = 0;
  s_kurve_beginn = BEGINN;
  s_kurve_leer = 0;
  s_kurve_luecke = false;
  s_alt_zahl = 0;
  s_vermerkt = -1;
  s_fertig = 0;
  telefon_zuruecksetzen();
}

void test_kieselsport_telefon__cleanup(void) {}

// --- Zustandsmeldungen ---

void test_kieselsport_telefon__schnelles_pause_weiter_kommt_ganz_an(void) {
  telefon_init();
  s_jetzt = BEGINN + 100;
  telefon_melde_zustand(ZustandPause, KS_ART, BEGINN);
  s_jetzt = BEGINN + 102;
  telefon_melde_zustand(ZustandWeiter, KS_ART, BEGINN);
  s_jetzt = BEGINN + 105;
  telefon_melde_zustand(ZustandPause, KS_ART, BEGINN);
  // Eine im Postausgang, zwei stehen an - keine ersetzt die andere.
  cl_assert_equal_i(s_anzahl, 1);
  prv_antwort(true);
  prv_warte(200);
  prv_antwort(true);
  prv_warte(200);
  cl_assert_equal_i(s_anzahl, 3);
  const int32_t was[] = { ZustandPause, ZustandWeiter, ZustandPause };
  const int32_t zeit[] = { 100, 102, 105 };
  for (int i = 0; i < 3; i++) {
    cl_assert_equal_i(prv_art(i), ArtZustand);
    cl_assert_equal_i(prv_feld(i, MESSAGE_KEY_ZUSTAND), was[i]);
    // Die Zeit des Drucks, nicht die der Ankunft.
    cl_assert_equal_i(prv_feld(i, MESSAGE_KEY_ZEIT), (int32_t)BEGINN + zeit[i]);
    cl_assert_equal_i(prv_feld(i, MESSAGE_KEY_BEGINN), (int32_t)BEGINN);
  }
}

void test_kieselsport_telefon__abgelehnt_noch_einmal_dann_die_naechste(void) {
  telefon_init();
  s_jetzt = BEGINN + 100;
  telefon_melde_zustand(ZustandPause, KS_ART, BEGINN);
  s_jetzt = BEGINN + 102;
  telefon_melde_zustand(ZustandWeiter, KS_ART, BEGINN);
  // Sechs Anlaeufe fuer die Pause, jedes Mal dieselbe Meldung mit ihrer Zeit.
  int pausen = 0;
  for (int i = 0; i < 20 && pausen < 6; i++) {
    if (s_belegt) {
      const int letzte = s_anzahl - 1;
      if (prv_art(letzte) == ArtZustand) {
        cl_assert_equal_i(prv_feld(letzte, MESSAGE_KEY_ZUSTAND), ZustandPause);
        cl_assert_equal_i(prv_feld(letzte, MESSAGE_KEY_ZEIT), (int32_t)BEGINN + 100);
        pausen++;
      }
      prv_antwort(false);
    }
    prv_warte(1500);
  }
  cl_assert_equal_i(pausen, 6);
  // Dann gibt sie auf, und das Weiter kommt dran - nicht verloren.
  for (int i = 0; i < 5 && !(s_belegt && prv_art(s_anzahl - 1) == ArtZustand); i++) {
    if (s_belegt) prv_antwort(false);
    prv_warte(1500);
  }
  cl_assert(s_belegt);
  cl_assert_equal_i(prv_feld(s_anzahl - 1, MESSAGE_KEY_ZUSTAND), ZustandWeiter);
  cl_assert_equal_i(prv_feld(s_anzahl - 1, MESSAGE_KEY_ZEIT), (int32_t)BEGINN + 102);
}

// --- Zusammenfassung ---

void test_kieselsport_telefon__zusammenfassung_wartet_auf_ihre_meldungen(void) {
  telefon_init();
  // Pause gedrueckt, das Telefon antwortet noch nicht - dann gespeichert.
  s_jetzt = BEGINN + 300;
  telefon_melde_zustand(ZustandPause, KS_ART, BEGINN);
  prv_zusammenfassung_wartet(BEGINN);
  telefon_nachsenden();
  prv_warte(3000);
  // Die Pause steht noch aus: die Zusammenfassung drueckt sich nicht vor.
  cl_assert_equal_i(s_anzahl, 1);
  prv_antwort(true);
  prv_warte(1000);
  cl_assert_equal_i(s_anzahl, 2);
  cl_assert_equal_i(prv_art(1), ArtZusammenfassung);
  // Sie traegt Pausen und Ende - und das Ende des Trainings.
  DictionaryIterator it;
  dict_read_begin_from_buffer(&it, s_gesendet[1].daten, s_gesendet[1].laenge);
  Tuple *p = dict_find(&it, MESSAGE_KEY_PAUSEN);
  cl_assert(p);
  cl_assert_equal_i(p->length, sizeof(s_pausen));
  cl_assert_equal_m(p->value->data, s_pausen, sizeof(s_pausen));
  cl_assert_equal_i(prv_feld(1, MESSAGE_KEY_ENDE), (int32_t)BEGINN + 300);
  cl_assert_equal_i(prv_feld(1, MESSAGE_KEY_ZUSTAND), ZustandStop);
  // Bestaetigt: im Persist vergessen, die App darf zu.
  telefon_bei_fertig(prv_fertig);
  prv_antwort(true);
  prv_warte(1000);
  cl_assert(!s_wartet);
  cl_assert_equal_i(s_fertig, 1);
}

void test_kieselsport_telefon__meldung_eines_anderen_trainings_haelt_nicht_auf(void) {
  telefon_init();
  // Ein Start des naechsten Trainings steht noch aus.
  s_jetzt = BEGINN + 2000;
  telefon_melde_zustand(ZustandStart, KS_ART, BEGINN + 2000);
  prv_zusammenfassung_wartet(BEGINN);
  telefon_nachsenden();
  prv_antwort(false);
  prv_warte(1000);
  // Die Zusammenfassung des alten geht trotzdem, sobald der Postausgang frei ist.
  bool zusammenfassung = false;
  for (int i = 0; i < 10 && !zusammenfassung; i++) {
    if (s_belegt) {
      zusammenfassung = prv_art(s_anzahl - 1) == ArtZusammenfassung;
      if (!zusammenfassung) prv_antwort(false);
    }
    prv_warte(1000);
  }
  cl_assert(zusammenfassung);
}

// --- Kurve ---

void test_kieselsport_telefon__die_kurve_laesst_meldungen_vor(void) {
  s_kurve_anzahl = 900;
  telefon_init();
  prv_warte(1000);
  cl_assert_equal_i(s_anzahl, 1);
  cl_assert_equal_i(prv_art(0), ArtKurve);
  // Waehrend das erste Stueck unterwegs ist, wird pausiert.
  s_jetzt = BEGINN + 50;
  telefon_melde_zustand(ZustandPause, KS_ART, BEGINN + 40);
  prv_antwort(true);
  prv_alles_bestaetigen();
  // Die Pause kam vor den restlichen Stuecken, und die ganze Kurve ist drueben.
  int pause = -1, kurve[3] = { -1, -1, -1 }, stuecke = 0;
  for (int i = 0; i < s_anzahl; i++) {
    if (prv_art(i) == ArtZustand && pause < 0) pause = i;
    if (prv_art(i) == ArtKurve && stuecke < 3) kurve[stuecke++] = i;
  }
  cl_assert_equal_i(stuecke, 3);
  cl_assert(kurve[0] < pause && pause < kurve[1]);
  cl_assert_equal_i(prv_feld(pause, MESSAGE_KEY_ZEIT), (int32_t)BEGINN + 50);
  cl_assert_equal_i(s_kurve_ab, 900);
}

// --- Der gemerkte Stopp ---

void test_kieselsport_telefon__stopp_aus_dem_persist_bis_bestaetigt(void) {
  s_stopp = true;
  s_stopp_beginn = BEGINN;
  s_stopp_zeit = BEGINN + 600;
  s_jetzt = BEGINN + 5000;
  telefon_bei_fertig(prv_fertig);
  telefon_init();
  cl_assert(telefon_stopp_wartet());
  prv_warte(300);
  cl_assert_equal_i(s_anzahl, 1);
  cl_assert_equal_i(prv_feld(0, MESSAGE_KEY_ZUSTAND), ZustandStop);
  // Die Zeit des Verwerfens, nicht die des Nachsendens.
  cl_assert_equal_i(prv_feld(0, MESSAGE_KEY_ZEIT), (int32_t)BEGINN + 600);
  // Das Telefon ist weg: alle Anlaeufe scheitern. Die App bleibt offen.
  for (int i = 0; i < 12; i++) {
    if (s_belegt) prv_antwort(false);
    prv_warte(1500);
  }
  cl_assert(s_stopp);
  cl_assert_equal_i(s_fertig, 0);
  // Es kommt wieder: der Stopp fasst nach, kommt an, die App geht zu.
  for (int i = 0; i < 20 && s_stopp; i++) {
    if (s_belegt) {
      if (prv_art(s_anzahl - 1) == ArtZustand) {
        cl_assert_equal_i(prv_feld(s_anzahl - 1, MESSAGE_KEY_ZEIT), (int32_t)BEGINN + 600);
      }
      prv_antwort(true);
    }
    prv_warte(1500);
  }
  cl_assert(!s_stopp);
  prv_warte(5000);
  cl_assert_equal_i(s_fertig, 1);
}

void test_kieselsport_telefon__stopp_steht_nur_einmal_an(void) {
  s_stopp = true;
  s_stopp_beginn = BEGINN;
  s_stopp_zeit = BEGINN + 600;
  telefon_init();
  telefon_stopp_nachsenden();
  telefon_stopp_nachsenden();
  prv_warte(300);
  prv_antwort(true);
  prv_warte(5000);
  int stopps = 0;
  for (int i = 0; i < s_anzahl; i++) {
    if (prv_art(i) == ArtZustand) stopps++;
  }
  cl_assert_equal_i(stopps, 1);
}

// --- Einstellungen vom Telefon ---

static DictionaryIterator *prv_eingang(int32_t maxpuls, int32_t gesetzt) {
  static uint8_t puffer[128];
  static DictionaryIterator lesen;
  DictionaryIterator schreiben;
  dict_write_begin(&schreiben, puffer, sizeof(puffer));
  if (maxpuls >= 0) dict_write_int32(&schreiben, MESSAGE_KEY_MAXPULS, maxpuls);
  if (gesetzt >= 0) dict_write_int32(&schreiben, MESSAGE_KEY_MAXPULS_GESETZT, gesetzt);
  const uint32_t n = dict_write_end(&schreiben);
  dict_read_begin_from_buffer(&lesen, puffer, (uint16_t)n);
  return &lesen;
}

void test_kieselsport_telefon__maxpuls_mit_und_ohne_vermerk(void) {
  telefon_init();
  // Ein altes Telefon: nur die Zahl.
  s_bei_eingang(prv_eingang(190, -1), NULL);
  cl_assert_equal_i(s_alt_zahl, 190);
  cl_assert_equal_i(s_vermerkt, -1);
  // Boulder mit Vermerk: "nach Alter", die Zahl daneben zaehlt nicht.
  s_alt_zahl = 0;
  s_bei_eingang(prv_eingang(179, 0), NULL);
  cl_assert_equal_i(s_vermerkt, 0);
  cl_assert_equal_i(s_alt_zahl, 0);
  s_bei_eingang(prv_eingang(190, 1), NULL);
  cl_assert_equal_i(s_vermerkt, 1);
  cl_assert_equal_i(s_vermerkt_zahl, 190);
  // Die Antwort traegt den neuen Stand samt Vermerk.
  prv_warte(3000);
  cl_assert(s_anzahl >= 1);
  cl_assert_equal_i(prv_art(s_anzahl - 1), ArtEinst);
  cl_assert_equal_i(prv_feld(s_anzahl - 1, MESSAGE_KEY_MAXPULS_GESETZT), 0);
}

// --- Kurve: ein Stueck, das sich nicht lesen laesst ---

// Wie oft eine Nachricht der Art [art] hinausging.
static int prv_zaehle(Nachrichtenart art) {
  int n = 0;
  for (int i = 0; i < s_anzahl; i++) {
    if (prv_art(i) == art) n++;
  }
  return n;
}

void test_kieselsport_telefon__leeres_kurvenstueck_blockiert_den_postausgang_nicht(void) {
  // Ein Lesefehler, der vorbeigeht: der Schluessel ist da, liest sich nur
  // einmal nicht.
  s_kurve_anzahl = 900;
  s_kurve_leer = 1;
  telefon_init();
  prv_warte(1000);
  // Danach muss der Start eines neuen Trainings hinausgehen - frueher blieb
  // der Postausgang nach dem leeren Stueck im Schreiben stehen.
  telefon_melde_zustand(ZustandStart, KS_ART, BEGINN + 3000);
  prv_warte(10000);
  cl_assert(!s_schreibt);
  cl_assert(s_anzahl >= 1);
  cl_assert_equal_i(prv_art(0), ArtZustand);
  cl_assert_equal_i(prv_feld(0, MESSAGE_KEY_ZUSTAND), ZustandStart);
  // Und die Kurve ist nicht vergessen: sie geht ganz hinaus.
  prv_alles_bestaetigen();
  cl_assert_equal_i(s_kurve_ab, 900);
  cl_assert_equal_i(prv_zaehle(ArtKurve), 3);
}

void test_kieselsport_telefon__kurve_mit_luecke_wird_verworfen(void) {
  // Der Schluessel des naechsten Stuecks fehlt: die Kurve kommt nie an.
  s_kurve_anzahl = 900;
  s_kurve_leer = 1000;
  s_kurve_luecke = true;
  telefon_bei_fertig(prv_fertig);
  telefon_init();
  prv_warte(3000);
  cl_assert_equal_i(s_kurve_anzahl, 0);
  cl_assert_equal_i(prv_zaehle(ArtKurve), 0);
  cl_assert(!s_schreibt);
  // Nichts wartet mehr: die App darf zu.
  prv_alles_bestaetigen();
  cl_assert_equal_i(s_fertig, 1);
}

void test_kieselsport_telefon__unlesbare_kurve_nach_einigen_versuchen_verworfen(void) {
  // Der Schluessel ist da, liest sich aber nie: ein paar Versuche, dann weg -
  // sonst stuende die App ohne Ende offen.
  s_kurve_anzahl = 900;
  s_kurve_leer = 1000;
  telefon_bei_fertig(prv_fertig);
  telefon_init();
  prv_warte(3000);
  cl_assert_equal_i(s_kurve_anzahl, 900);
  prv_warte(30000);
  cl_assert_equal_i(s_kurve_anzahl, 0);
  prv_alles_bestaetigen();
  cl_assert_equal_i(s_fertig, 1);
}

// --- Bestaetigungen gehoeren zu ihrem Training ---

void test_kieselsport_telefon__spaete_bestaetigung_loescht_kein_neues_training(void) {
  prv_zusammenfassung_wartet(BEGINN);
  telefon_init();
  prv_warte(1000);
  cl_assert_equal_i(s_anzahl, 1);
  cl_assert_equal_i(prv_art(0), ArtZusammenfassung);
  // Waehrend A beim Telefon ist, wird B gespeichert - die App schickt nach.
  prv_zusammenfassung_wartet(BEGINN + 5000);
  telefon_nachsenden();
  // Jetzt kommt die Bestaetigung fuer A.
  prv_antwort(true);
  prv_warte(2000);
  cl_assert(s_wartet);
  cl_assert_equal_i(s_anzahl, 2);
  cl_assert_equal_i(prv_art(1), ArtZusammenfassung);
  cl_assert_equal_i(prv_feld(1, MESSAGE_KEY_BEGINN), (int32_t)(BEGINN + 5000));
  prv_antwort(true);
  prv_warte(1000);
  cl_assert(!s_wartet);
}

void test_kieselsport_telefon__spaete_kurvenbestaetigung_trifft_keine_neue_kurve(void) {
  s_kurve_anzahl = 900;
  telefon_init();
  prv_warte(1000);
  cl_assert_equal_i(prv_art(0), ArtKurve);
  // Waehrend das erste Stueck von A unterwegs ist, liegt die Kurve von B da.
  s_kurve_beginn = BEGINN + 5000;
  s_kurve_anzahl = 400;
  s_kurve_ab = 0;
  prv_antwort(true);
  // Die Bestaetigung von A rueckt B nicht vor: B geht von vorn.
  cl_assert_equal_i(s_kurve_ab, 0);
  prv_warte(1000);
  cl_assert_equal_i(s_anzahl, 2);
  cl_assert_equal_i(prv_feld(1, MESSAGE_KEY_BEGINN), (int32_t)(BEGINN + 5000));
  cl_assert_equal_i(prv_feld(1, MESSAGE_KEY_KURVE_AB), 0);
  prv_alles_bestaetigen();
  cl_assert_equal_i(s_kurve_ab, 400);
}

// --- Ohne Telefon ---

void test_kieselsport_telefon__ohne_telefon_selten_versuchen(void) {
  s_verbunden = false;
  prv_zusammenfassung_wartet(BEGINN);
  s_kurve_anzahl = 900;
  telefon_init();
  telefon_melde_zustand(ZustandStart, KS_ART, BEGINN + 3000);
  // Eine Minute ohne Telefon: was hinausginge, scheiterte.
  for (int i = 0; i < 120; i++) {
    if (s_belegt) prv_antwort(false);
    prv_warte(500);
  }
  cl_assert(s_begin_versuche <= 6);
  // Das Telefon ist wieder da: binnen zehn Sekunden geht es los.
  const int vorher = s_anzahl;
  s_verbunden = true;
  prv_warte(10000);
  cl_assert(s_anzahl > vorher);
  // Der Start kam nicht um seine Anlaeufe: er geht noch.
  prv_alles_bestaetigen();
  bool start = false;
  for (int i = vorher; i < s_anzahl; i++) {
    if (prv_art(i) == ArtZustand && prv_feld(i, MESSAGE_KEY_ZUSTAND) == ZustandStart) start = true;
  }
  cl_assert(start);
  cl_assert(!s_wartet);
}

// --- Postausgang nicht offen ---

void test_kieselsport_telefon__postausgang_nicht_offen_keine_endlosschleife(void) {
  s_open_ergebnis = APP_MSG_OUT_OF_MEMORY;
  prv_zusammenfassung_wartet(BEGINN);
  s_kurve_anzahl = 900;
  telefon_init();
  telefon_melde_zustand(ZustandStart, KS_ART, BEGINN + 3000);
  prv_warte(60000);
  cl_assert_equal_i(s_anzahl, 0);
  // Kein Wecker mehr: nichts versucht es weiter, die Daten bleiben im Persist.
  cl_assert(!prv_wecker_offen());
  cl_assert(s_wartet);
  cl_assert_equal_i(s_kurve_anzahl, 900);
}

// --- Zeiten ohne Vorzeichen ---

static Tuple prv_tupel(int i, uint32_t key) {
  DictionaryIterator it;
  dict_read_begin_from_buffer(&it, s_gesendet[i].daten, s_gesendet[i].laenge);
  Tuple *t = dict_find(&it, key);
  cl_assert(t);
  return *t;
}

void test_kieselsport_telefon__zeiten_nach_2038_ohne_vorzeichen(void) {
  // 18.9.2039: als int32 waere das negativ.
  const uint32_t spaet = 2200000000u;
  s_jetzt = (time_t)spaet + 10;
  telefon_init();
  telefon_melde_zustand(ZustandStart, KS_ART, spaet);
  cl_assert_equal_i(s_anzahl, 1);
  Tuple b = prv_tupel(0, MESSAGE_KEY_BEGINN);
  cl_assert_equal_i(b.type, TUPLE_UINT);
  cl_assert_equal_i(b.length, 4);
  Tuple z = prv_tupel(0, MESSAGE_KEY_ZEIT);
  cl_assert_equal_i(z.type, TUPLE_UINT);
  prv_antwort(true);
  // Die Zusammenfassung traegt Beginn und Ende, die Kurve den Beginn.
  s_wartet = true;
  s_stand = (Trainingsstand){ .art = KS_ART, .beginn = spaet, .dauer_s = 240 };
  s_kurve_beginn = spaet;
  s_kurve_anzahl = 10;
  telefon_nachsenden();
  prv_alles_bestaetigen();
  int zusammenfassung = -1, kurve = -1;
  for (int i = 0; i < s_anzahl; i++) {
    if (prv_art(i) == ArtZusammenfassung) zusammenfassung = i;
    if (prv_art(i) == ArtKurve) kurve = i;
  }
  cl_assert(zusammenfassung >= 0 && kurve >= 0);
  DictionaryIterator it;
  dict_read_begin_from_buffer(&it, s_gesendet[zusammenfassung].daten,
                              s_gesendet[zusammenfassung].laenge);
  Tuple *zb = dict_find(&it, MESSAGE_KEY_BEGINN);
  cl_assert(zb && zb->type == TUPLE_UINT);
  cl_assert_equal_i(zb->value->uint32, spaet);
  Tuple ze = prv_tupel(zusammenfassung, MESSAGE_KEY_ENDE);
  cl_assert_equal_i(ze.type, TUPLE_UINT);
  Tuple kb = prv_tupel(kurve, MESSAGE_KEY_BEGINN);
  cl_assert_equal_i(kb.type, TUPLE_UINT);
}

// --- Das Echo der Einstellungen ---

void test_kieselsport_telefon__einstellungsecho_wartet_hinter_langer_kurve(void) {
  // Vier Stuecke Kurve, das Telefon braucht je vier Sekunden fuer die
  // Antwort: der Postausgang ist lange besetzt.
  s_kurve_anzahl = 1200;
  telefon_init();
  for (int i = 0; i < 60 && s_kurve_ab < s_kurve_anzahl; i++) {
    prv_warte(4000);
    if (s_belegt) prv_antwort(true);
  }
  cl_assert_equal_i(s_kurve_ab, 1200);
  prv_alles_bestaetigen();
  // Besetzt war kein Anlauf: das Echo kam danach noch.
  cl_assert_equal_i(prv_zaehle(ArtEinst), 1);
}
