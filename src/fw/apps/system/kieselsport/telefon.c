/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include <string.h>
#include "telefon.h"
#include "einstellungen.h"
#include "meldungen.h"
#include "schluessel.h"

// Der Postausgang traegt die ganze Zusammenfassung: die Abschnittsliste (bis
// zu zwanzig Saetze oder Bahnen als Text, ~310 Byte), die Pausen (bis 137
// Byte) und vierzehn Zahlen (154 Byte) - knapp 600. 636 ist das Groesste,
// was jede Pebble-App ohne 8-KB-Nachrichten oeffnen darf.
#define INBOX_SIZE 256
#define OUTBOX_SIZE 636

// Ein zweiter Anlauf, falls der Postausgang gerade besetzt ist. Er fasst
// genau EINE Nachricht; kommt die Zusammenfassung zu dicht hinter etwas
// anderem, fiele sie still mit BUSY aus - und das Training wäre weg.
static AppTimer *s_nachfassen;
static Trainingsstand s_wartet;
static char s_liste[KS_LISTE_MAX];
static uint8_t s_pausen[KS_PAUSEN_BYTES];
static size_t s_pausen_laenge;
static uint32_t s_ende;
static bool s_hat_wartende;
// WAS ZULETZT HINAUSGING. Der Postausgang fasst genau eine Nachricht, und
// Bestaetigung wie Ablehnung sagen nicht, welche es war - nur die
// Bestaetigung der Zusammenfassung darf die Wartende loeschen, nur die eines
// Kurvenstuecks den Zeiger der Kurve vorruecken. Frueher standen hier drei
// Schalter; mit Nacht und Einzelmessung waeren es fuenf geworden.
typedef enum {
  SendungNichts = 0,
  SendungZusammenfassung,
  SendungZustand,
  SendungKurve,
  SendungEinst,
} Sendung;
static Sendung s_letzte;
// Der Beginn der Zusammenfassung, die gerade unterwegs ist. Ihre
// Bestaetigung loescht nur sie - kam inzwischen ein neueres Training in den
// Persist, bleibt das liegen und geht als naechstes.
static uint32_t s_unterwegs_beginn;

// OHNE TELEFON KEIN ANLAUF. Ist es nicht verbunden, scheitert jede Nachricht
// ohnehin; statt alle 700 ms den Postausgang zu bemuehen, wird alle zehn
// Sekunden nachgesehen. Gelesen wird nur der Zustand der Verbindung.
#define VERBINDUNG_WARTEN_MS 10000
static bool prv_telefon_da(void) {
  return connection_service_peek_pebble_app_connection();
}

// Ging app_message_open schief (kein Speicher), ist der Postausgang zu und
// jedes begin scheitert. Dann wird nichts mehr versucht - die Daten bleiben
// im Persist, der Dienst holt die App spaeter wieder.
static bool s_postausgang_zu;

static void prv_abgelehnt(DictionaryIterator *iter, AppMessageResult grund, void *context);

// Nach einem gelungenen begin scheitert send praktisch nicht. Wenn doch,
// steht es im Log und gilt als abgelehnt - sonst wartete die Schlange auf
// eine Antwort zu einer Nachricht, die nie hinausging.
static void prv_senden(void) {
  const AppMessageResult r = app_message_outbox_send();
  if (r != APP_MSG_OK) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Senden gescheitert: %d", (int)r);
    prv_abgelehnt(NULL, r, NULL);
  }
}

// Die Zustandsmeldungen, der Reihe nach (meldungen.h). Ein paar Anlaeufe je
// Meldung, nicht endlos: die Zusammenfassung am Ende sagt alles noch einmal.
static Meldungen s_meldungen;
static AppTimer *s_zustand_timer;
#define ZUSTAND_VERSUCHE 6
#define ZUSTAND_ABSTAND_MS 1500

static void prv_zustand_senden(void *data);

// Wenn nichts mehr wartet (telefon_bei_fertig). AUCH KEINE MELDUNG MEHR:
// sonst ginge die App, die nur fuer einen Stopp geholt wurde, ohne ihn zu -
// und ein aufgegebener Stopp liegt noch im Persist, ist also nicht drueben.
static void (*s_fertig)(void);
// DER GEMERKTE STOPP FASST NACH WIE DIE ZUSAMMENFASSUNG. Hat er seine
// Anlaeufe verbraucht (Telefon weg), liegt er noch im Persist; ohne neuen
// Anlauf stuende die App, die fuer ihn geholt wurde, bis jemand sie
// schliesst - auch wenn das Telefon laengst wieder da ist.
static AppTimer *s_stopp_timer;
#define STOPP_WIEDER_MS 2000
static void prv_stopp_wieder(void *data) {
  s_stopp_timer = NULL;
  telefon_stopp_nachsenden();
}
static void prv_alles_drueben(void) {
  if (!meldungen_leer(&s_meldungen) || s_hat_wartende || kieselsport_kurve_wartet()) {
    return;
  }
  if (telefon_stopp_wartet()) {
    if (!s_stopp_timer) s_stopp_timer = app_timer_register(STOPP_WIEDER_MS, prv_stopp_wieder, NULL);
    return;
  }
  if (s_fertig) s_fertig();
}

// --- Die Pulskurve, stueckweise ---
//
// Erst wenn die Zusammenfassung bestaetigt ist: sie ist das Wichtige, und der
// Postausgang fasst genau eine Nachricht. Dann je Nachricht bis zu 300 Werte
// (fuenfzig Minuten), bis alles drueben ist. Jedes bestaetigte Stueck rueckt
// den Zeiger im Persist vor - geht die App dazwischen zu, geht es beim
// naechsten Oeffnen dort weiter.
#define KURVE_JE_NACHRICHT 300
static uint16_t s_kurve_unterwegs;
// Wozu das Stueck gehoert, das unterwegs ist: die Bestaetigung rueckt nur
// diese Kurve ab dieser Stelle vor (kieselsport_kurve_bestaetigt).
static uint32_t s_kurve_beginn_unterwegs;
static uint16_t s_kurve_ab_unterwegs;
static AppTimer *s_kurve_timer;
// EIN LESEFEHLER IST NICHT GLEICH EINE KAPUTTE KURVE. Fehlt der Schluessel,
// ist sie es; liest er sich nur gerade nicht, wird es ein paarmal wieder
// versucht, bevor sie wegfaellt - sonst stuende die App ohne Ende offen.
#define KURVE_LESEVERSUCHE 5
#define KURVE_LESEN_WIEDER_MS 2000
static uint8_t s_kurve_lesefehler;


static void prv_kurve_senden(void *data);

// Das naechste Stueck laesst sich nicht lesen.
static void prv_kurve_unlesbar(uint16_t ab) {
  if (!kieselsport_kurve_luecke() && ++s_kurve_lesefehler < KURVE_LESEVERSUCHE) {
    APP_LOG(APP_LOG_LEVEL_WARNING, "Kurve ab %u nicht lesbar, neuer Versuch", (unsigned)ab);
    s_kurve_timer = app_timer_register(KURVE_LESEN_WIEDER_MS, prv_kurve_senden, NULL);
    return;
  }
  APP_LOG(APP_LOG_LEVEL_ERROR, "Kurve ab %u kaputt, verworfen", (unsigned)ab);
  s_kurve_lesefehler = 0;
  kieselsport_kurve_vergessen();
  prv_alles_drueben();
}

static void prv_kurve_senden(void *data) {
  s_kurve_timer = NULL;
  if (s_hat_wartende || s_postausgang_zu) return;
  if (!kieselsport_kurve_wartet()) { prv_alles_drueben(); return; }
  if (!prv_telefon_da()) {
    s_kurve_timer = app_timer_register(VERBINDUNG_WARTEN_MS, prv_kurve_senden, NULL);
    return;
  }
  // DIE MELDUNGEN ZUERST: ein Start, der hinter Kurvenstuecken anstuende,
  // kaeme nach sechs besetzten Anlaeufen nie an - und die Strecke fehlte.
  if (!meldungen_leer(&s_meldungen)) {
    s_kurve_timer = app_timer_register(700, prv_kurve_senden, NULL);
    return;
  }
  // ERST LESEN, DANN DEN POSTAUSGANG OEFFNEN. Ein begin ohne send laesst ihn
  // im Zustand "wird geschrieben" stehen, und jedes weitere begin scheitert -
  // kein Start, keine Pause, keine Zusammenfassung ginge mehr hinaus, bis
  // die App zugeht. Ein leeres Stueck darf also gar nicht erst dort landen.
  // Kopf und Werte in einem Griff: sonst kaemen mitten im Speichern eines
  // neuen Trainings dessen Werte unter dem Beginn des alten hinaus.
  static uint8_t stueck[KURVE_JE_NACHRICHT];
  uint32_t beginn;
  uint16_t ab, anzahl;
  const uint16_t n = kieselsport_kurve_lesen(&beginn, &ab, &anzahl, stueck, sizeof(stueck));
  if (n == 0) {
    prv_kurve_unlesbar(ab);
    return;
  }
  s_kurve_lesefehler = 0;
  DictionaryIterator *out;
  if (app_message_outbox_begin(&out) != APP_MSG_OK) {
    s_kurve_timer = app_timer_register(700, prv_kurve_senden, NULL);
    return;
  }
  dict_write_uint32(out, MESSAGE_KEY_BEGINN, beginn);
  dict_write_int32(out, MESSAGE_KEY_KURVE_ANZAHL, (int32_t)anzahl);
  dict_write_int32(out, MESSAGE_KEY_KURVE_AB, (int32_t)ab);
  dict_write_data(out, MESSAGE_KEY_KURVE, stueck, n);
  s_kurve_unterwegs = n;
  s_kurve_beginn_unterwegs = beginn;
  s_kurve_ab_unterwegs = ab;
  s_letzte = SendungKurve;
  prv_senden();
}

static void prv_sende_jetzt(void);

// --- Die Einstellungen ans Telefon ---
//
// Beim Start und nach jeder Aenderung. Hinter allem anderen: die
// Zusammenfassung und die Kurve gehen vor, die Meldung wartet, bis der
// Postausgang frei ist.
static bool s_einst_offen;
static AppTimer *s_einst_timer;
static uint8_t s_einst_versuche;

static void prv_einst_senden(void *data) {
  s_einst_timer = NULL;
  if (!s_einst_offen || s_postausgang_zu) return;
  if (s_einst_versuche >= 6) { s_einst_offen = false; return; }
  if (!prv_telefon_da()) {
    s_einst_timer = app_timer_register(VERBINDUNG_WARTEN_MS, prv_einst_senden, NULL);
    return;
  }
  // BESETZT IST KEIN ANLAUF, wie bei den Zustandsmeldungen: geht gerade eine
  // lange Kurve hinaus, fiele das Echo sonst nach neun Sekunden weg, und
  // Boulder meldete "keine Antwort", obwohl die Uhr die Werte hat.
  DictionaryIterator *out;
  if (s_hat_wartende || app_message_outbox_begin(&out) != APP_MSG_OK) {
    s_einst_timer = app_timer_register(1500, prv_einst_senden, NULL);
    return;
  }
  s_einst_versuche++;
  einstellungen_melden(out);
  s_letzte = SendungEinst;
  prv_senden();
}

static void prv_einst_vormerken(uint32_t ms) {
  s_einst_offen = true;
  s_einst_versuche = 0;
  if (s_einst_timer) app_timer_cancel(s_einst_timer);
  s_einst_timer = app_timer_register(ms, prv_einst_senden, NULL);
}

static void prv_nachfassen(void *data) {
  s_nachfassen = NULL;
  if (!s_hat_wartende || s_postausgang_zu) return;
  if (!prv_telefon_da()) {
    s_nachfassen = app_timer_register(VERBINDUNG_WARTEN_MS, prv_nachfassen, NULL);
    return;
  }
  // ERST DIE MELDUNGEN DIESES TRAININGS: ein Weiter, das nach der
  // Zusammenfassung ankaeme, startete das GPS fuer ein beendetes Training.
  if (meldungen_fuer(&s_meldungen, s_wartet.beginn)) {
    s_nachfassen = app_timer_register(700, prv_nachfassen, NULL);
    return;
  }
  prv_sende_jetzt();
}

/**
 * Clay schickt Zahlen mal als Zahl, mal als Zeichenkette - beides nehmen.
 * Alles andere (Bytes, zu kurze Zahlen) ist 0: sonst laese das vier Bytes,
 * wo nur eines liegt.
 */
static int32_t prv_zahl(Tuple *t) {
  switch (t->type) {
    case TUPLE_CSTRING:
      return t->length > 0 && strnlen(t->value->cstring, t->length) < t->length
                 ? atoi(t->value->cstring) : 0;
    case TUPLE_INT:
      return t->length == 1 ? t->value->int8 : t->length == 2 ? t->value->int16 :
             t->length == 4 ? t->value->int32 : 0;
    case TUPLE_UINT:
      return t->length == 1 ? t->value->uint8 : t->length == 2 ? t->value->uint16 :
             t->length == 4 ? (int32_t)t->value->uint32 : 0;
    default:
      return 0;
  }
}

static void prv_inbox(DictionaryIterator *iter, void *context) {
  Tuple *max = dict_find(iter, MESSAGE_KEY_MAXPULS);
  // MIT VERMERK sagt das Telefon, ob es einen Wert meint oder "nach Alter";
  // ohne ist es eines, das das noch nicht kennt - dann wie bisher.
  Tuple *gesetzt = dict_find(iter, MESSAGE_KEY_MAXPULS_GESETZT);
  if (gesetzt) {
    einstellungen_maxpuls_vermerkt(prv_zahl(gesetzt) != 0, max ? prv_zahl(max) : 0);
  } else if (max) {
    einstellungen_maxpuls(prv_zahl(max));
  }

  Tuple *becken = dict_find(iter, MESSAGE_KEY_BECKEN);
  if (becken) einstellungen_becken(prv_zahl(becken));

  Tuple *ziel = dict_find(iter, MESSAGE_KEY_PAUSENZIEL);
  if (ziel) einstellungen_pausenziel(prv_zahl(ziel));

  Tuple *empf = dict_find(iter, MESSAGE_KEY_EMPFIND);
  if (empf) einstellungen_empfindlichkeit(prv_zahl(empf));

  Tuple *pin_art = dict_find(iter, MESSAGE_KEY_PIN_ART);
  if (pin_art) einstellungen_pin_art(prv_zahl(pin_art));

  Tuple *pin_zeit = dict_find(iter, MESSAGE_KEY_PIN_ZEIT);
  // Nur eine abgeschlossene Zeichenkette - strlen liefe sonst hinter das Ende.
  if (pin_zeit && pin_zeit->type == TUPLE_CSTRING && pin_zeit->length > 0 &&
      strnlen(pin_zeit->value->cstring, pin_zeit->length) < pin_zeit->length) {
    einstellungen_pin_zeit(pin_zeit->value->cstring);
  }

  // DIE NACHT KOMMT NICHT MEHR HIERHER: die misst jetzt die Firmware. Was
  // Kiesel-Helper dazu schickt, bleibt liegen - die Antwort sagt ihm, dass
  // sie hier aus ist.
  Tuple *nacht = dict_find(iter, MESSAGE_KEY_NACHT_AN);

  // Der neue Stand an beide Seiten - Konfigseite und Kiesel-Helper.
  if (max || gesetzt || becken || ziel || empf || pin_art || pin_zeit || nacht) {
    prv_einst_vormerken(300);
  }
}

static void prv_abgelehnt(DictionaryIterator *iter, AppMessageResult grund, void *context) {
  APP_LOG(APP_LOG_LEVEL_WARNING, "Nachricht abgelehnt: %d", (int)grund);
  const Sendung war = s_letzte;
  s_letzte = SendungNichts;
  switch (war) {
    case SendungEinst:
      if (s_einst_offen && !s_einst_timer) s_einst_timer = app_timer_register(2000, prv_einst_senden, NULL);
      return;
    case SendungKurve:
      if (!s_kurve_timer) s_kurve_timer = app_timer_register(2000, prv_kurve_senden, NULL);
      break;
    case SendungZustand:
      // Dieselbe Meldung noch einmal - die naechste wartet hinter ihr.
      s_meldungen.unterwegs = false;
      if (!s_zustand_timer) {
        s_zustand_timer = app_timer_register(ZUSTAND_ABSTAND_MS, prv_zustand_senden, NULL);
      }
      break;
    default:
      break;
  }
  if (s_hat_wartende && !s_nachfassen) {
    s_nachfassen = app_timer_register(2000, prv_nachfassen, NULL);
  }
}

static void prv_angekommen(DictionaryIterator *iter, void *context) {
  const Sendung war = s_letzte;
  s_letzte = SendungNichts;
  switch (war) {
    case SendungEinst:
      s_einst_offen = false;
      return;
    case SendungKurve:
      if (kieselsport_kurve_bestaetigt(s_kurve_beginn_unterwegs, s_kurve_ab_unterwegs,
                                       s_kurve_unterwegs)) {
        APP_LOG(APP_LOG_LEVEL_INFO, "Kurve: %u Werte bestaetigt", (unsigned)s_kurve_unterwegs);
      } else {
        APP_LOG(APP_LOG_LEVEL_WARNING, "Kurve: Bestaetigung passt nicht mehr, ignoriert");
      }
      if (kieselsport_kurve_wartet()) {
        if (!s_kurve_timer) s_kurve_timer = app_timer_register(150, prv_kurve_senden, NULL);
      } else {
        prv_alles_drueben();
      }
      return;
    case SendungZustand: {
      // Der gemerkte Stopp ist jetzt drueben - aus dem Persist damit.
      const Meldung *m = meldungen_vorn(&s_meldungen);
      if (m && m->aus_persist) kieselsport_stopp_vergessen(m->beginn);
      meldungen_vorn_weg(&s_meldungen);
      if (!s_zustand_timer) s_zustand_timer = app_timer_register(100, prv_zustand_senden, NULL);
      return;
    }
    case SendungZusammenfassung:
      break;
    default:
      return;
  }
  s_hat_wartende = false;
  kieselsport_wartend_vergessen(s_unterwegs_beginn);
  APP_LOG(APP_LOG_LEVEL_INFO, "Zusammenfassung bestaetigt");
  // WARTET SCHON DIE NAECHSTE? Wurde waehrend der Antwort ein neues
  // Training gespeichert, ist es noch da - es geht jetzt, die Kurve danach.
  // Dieselbe noch einmal nicht: liess sie sich nicht loeschen, ginge sie
  // sonst ohne Ende hin und her.
  telefon_nachsenden();
  if (s_hat_wartende && s_wartet.beginn != s_unterwegs_beginn) return;
  s_hat_wartende = false;
  // Jetzt die Kurve hinterher.
  if (kieselsport_kurve_wartet()) {
    if (!s_kurve_timer) s_kurve_timer = app_timer_register(150, prv_kurve_senden, NULL);
  } else {
    prv_alles_drueben();
  }
}

static void prv_sende_jetzt(void) {
  DictionaryIterator *out;
  if (app_message_outbox_begin(&out) != APP_MSG_OK) {
    if (!s_nachfassen) s_nachfassen = app_timer_register(700, prv_nachfassen, NULL);
    return;
  }
  // ZEITEN OHNE VORZEICHEN: als int32 kaemen sie ab 2038 negativ an. Boulder
  // liest beide Arten von Zahl gleich (KieselsportBegleiter.zerlege).
  dict_write_int32(out, MESSAGE_KEY_ART, (int32_t)s_wartet.art);
  dict_write_uint32(out, MESSAGE_KEY_BEGINN, s_wartet.beginn);
  dict_write_int32(out, MESSAGE_KEY_DAUER, (int32_t)s_wartet.dauer_s);
  dict_write_int32(out, MESSAGE_KEY_SCHRITTE, (int32_t)s_wartet.schritte);
  dict_write_int32(out, MESSAGE_KEY_METER, (int32_t)s_wartet.meter);
  dict_write_int32(out, MESSAGE_KEY_KCAL, (int32_t)s_wartet.kcal);
  dict_write_int32(out, MESSAGE_KEY_PULS_MITTEL, (int32_t)s_wartet.puls_mittel);
  dict_write_int32(out, MESSAGE_KEY_PULS_MAX, (int32_t)s_wartet.puls_max);
  dict_write_int32(out, MESSAGE_KEY_SAETZE, (int32_t)s_wartet.saetze);
  dict_write_int32(out, MESSAGE_KEY_REPS, (int32_t)s_wartet.reps);
  dict_write_int32(out, MESSAGE_KEY_BAHNEN, (int32_t)s_wartet.bahnen);
  // Nur bei Yoga - und nur, wenn genug Schlaege fuer eine Zahl da waren.
  if (s_wartet.hrv_ms > 0) {
    dict_write_int32(out, MESSAGE_KEY_HRV, (int32_t)s_wartet.hrv_ms);
  }
  // DIE LISTE IST DAS, WAS DEN TAG SPAETER ERKLAERT. "4 Saetze" sagt wenig,
  // "12/10/8/8 mit 90 Sekunden dazwischen" sagt alles - und auf dem Telefon
  // wird jeder Abschnitt ein eigener Eintrag in der Gesundheitsakte.
  if (s_liste[0]) {
    dict_write_cstring(out, MESSAGE_KEY_ABSCHNITTE, s_liste);
  }
  // DIE PAUSEN UND DAS ENDE DER AKTIVEN ZEIT. Damit rechnet das Telefon
  // Kurve, Saetze und Bahnen in Uhrzeit um, ohne sich auf die Ankunft von
  // Pause und Weiter zu verlassen - und schneidet die Strecke dort ab, wo
  // gespeichert wurde, statt zwanzig Minuten Puffer zu lassen.
  if (s_pausen_laenge > 0) {
    dict_write_data(out, MESSAGE_KEY_PAUSEN, s_pausen, s_pausen_laenge);
  }
  if (s_ende > 0) {
    dict_write_uint32(out, MESSAGE_KEY_ENDE, s_ende);
  }
  // DAS ENDE STEHT IN DERSELBEN NACHRICHT. Eine eigene Stopmeldung daneben
  // straeubte sich mit dieser um den Postausgang - der fasst genau EINE
  // Nachricht, und die zweite fiele mit BUSY aus.
  dict_write_int32(out, MESSAGE_KEY_ZUSTAND, (int32_t)ZustandStop);
  s_unterwegs_beginn = s_wartet.beginn;
  s_letzte = SendungZusammenfassung;
  prv_senden();
}

bool telefon_wartet(void) { return s_hat_wartende || kieselsport_kurve_wartet(); }

void telefon_nachsenden(void) {
  // Ein Stopp, der noch auf das Telefon wartet, zuerst - er ist kurz.
  telefon_stopp_nachsenden();
  if (!kieselsport_wartend_laden(&s_wartet, s_liste, sizeof(s_liste))) return;
  s_pausen_laenge = kieselsport_wartend_pausen(s_pausen, sizeof(s_pausen));
  s_ende = kieselsport_wartend_ende();
  s_hat_wartende = true;
  APP_LOG(APP_LOG_LEVEL_INFO, "Zusammenfassung geht ans Telefon");
  // Mit etwas Abstand, damit die Verbindung zum Telefon erst steht.
  if (!s_nachfassen) s_nachfassen = app_timer_register(600, prv_nachfassen, NULL);
}

/**
 * Die vordere Zustandsmeldung - mit ein paar Anlaeufen.
 *
 * DER START MUSS ANKOMMEN, sonst fehlt die Strecke: das Telefon zeichnet
 * nur auf, wenn es weiss, dass ein Training laeuft. Der erste Entwurf
 * schickte die Meldung genau einmal; war die Verbindung in dieser Sekunde
 * gerade besetzt, gab es ein Wandern ohne Karte, und niemand wusste, warum.
 * Jetzt fasst sie nach - ein paarmal, nicht endlos: die Zusammenfassung am
 * Ende sagt alles noch einmal, und die fasst so lange nach, bis sie da ist.
 */
static void prv_zustand_senden(void *data) {
  s_zustand_timer = NULL;
  // Eine ist schon unterwegs - ihre Antwort stoesst die naechste an.
  if (s_meldungen.unterwegs || s_postausgang_zu) return;
  while (!meldungen_leer(&s_meldungen) && s_meldungen.anlaeufe >= ZUSTAND_VERSUCHE) {
    APP_LOG(APP_LOG_LEVEL_WARNING, "Zustandsmeldung aufgegeben");
    meldungen_vorn_weg(&s_meldungen);
  }
  const Meldung *m = meldungen_vorn(&s_meldungen);
  if (!m) {
    prv_alles_drueben();
    return;
  }
  // OHNE TELEFON AUCH KEIN ANLAUF: die Meldung wartet, bis es wieder da ist,
  // statt ihre sechs Versuche ins Leere zu schicken. Mit ihrer Uhrzeit
  // kommt sie auch spaet noch richtig an.
  if (!prv_telefon_da()) {
    s_zustand_timer = app_timer_register(VERBINDUNG_WARTEN_MS, prv_zustand_senden, NULL);
    return;
  }
  DictionaryIterator *out;
  // BESETZT IST KEIN ANLAUF: dann ist gerade die Kurve, die Zusammenfassung
  // oder der Stand der Einstellungen unterwegs, die Meldung kam gar nicht
  // hinaus. Gezaehlt wird, was das Telefon nicht annahm.
  if (app_message_outbox_begin(&out) != APP_MSG_OK) {
    s_zustand_timer = app_timer_register(ZUSTAND_ABSTAND_MS, prv_zustand_senden, NULL);
    return;
  }
  meldungen_anlauf(&s_meldungen, ZUSTAND_VERSUCHE);
  dict_write_int32(out, MESSAGE_KEY_ZUSTAND, (int32_t)m->was);
  dict_write_int32(out, MESSAGE_KEY_ART, (int32_t)m->art);
  // DER BEGINN MUSS SCHON HIER MIT. Das Telefon legt die Spurdatei unter
  // diesem Zeitpunkt ab; erfuehre es ihn erst mit der Zusammenfassung,
  // haette es die Punkte unter einem anderen Namen gesammelt.
  dict_write_uint32(out, MESSAGE_KEY_BEGINN, m->beginn);
  // Wann es geschah: eine Meldung im dritten Anlauf kommt Sekunden spaeter.
  dict_write_uint32(out, MESSAGE_KEY_ZEIT, m->zeit);
  s_meldungen.unterwegs = true;
  s_letzte = SendungZustand;
  prv_senden();
}

static void prv_zustand_anstossen(uint32_t ms) {
  if (!s_zustand_timer && !s_meldungen.unterwegs) {
    s_zustand_timer = app_timer_register(ms, prv_zustand_senden, NULL);
  }
}

void telefon_melde_zustand(Trainingsmeldung was, uint8_t art, uint32_t beginn) {
  // HINTEN ANSTELLEN, NICHT ERSETZEN: ein schnelles Pause-Weiter verdraengte
  // sonst die Pause, und das Telefon verwarf alle.
  meldungen_anstellen(&s_meldungen, (Meldung){
    .was = (uint8_t)was, .art = art, .beginn = beginn, .zeit = (uint32_t)time(NULL),
  });
  // Gleich los, wenn nichts unterwegs ist: die Aufzeichnung soll mit dem
  // Druck anhalten, nicht nach dem naechsten Anlauf.
  if (!s_meldungen.unterwegs) {
    if (s_zustand_timer) { app_timer_cancel(s_zustand_timer); s_zustand_timer = NULL; }
    prv_zustand_senden(NULL);
  }
}

void telefon_stopp_nachsenden(void) {
  uint32_t beginn = 0, zeit = 0;
  uint8_t art = 0;
  if (!kieselsport_stopp_laden(&beginn, &zeit, &art) || meldungen_hat_stopp(&s_meldungen, beginn)) {
    return;
  }
  meldungen_anstellen(&s_meldungen, (Meldung){
    .was = ZustandStop, .art = art, .beginn = beginn, .zeit = zeit, .aus_persist = true,
  });
  // Mit etwas Abstand, damit die Verbindung zum Telefon erst steht.
  prv_zustand_anstossen(300);
}

bool telefon_stopp_wartet(void) {
  uint32_t beginn = 0, zeit = 0;
  uint8_t art = 0;
  return kieselsport_stopp_laden(&beginn, &zeit, &art);
}

void telefon_bei_fertig(void (*fertig)(void)) { s_fertig = fertig; }

void telefon_init(void) {
  app_message_register_inbox_received(prv_inbox);
  app_message_register_outbox_failed(prv_abgelehnt);
  app_message_register_outbox_sent(prv_angekommen);
  const AppMessageResult offen = app_message_open(INBOX_SIZE, OUTBOX_SIZE);
  if (offen != APP_MSG_OK) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Postausgang nicht offen: %d", (int)offen);
    s_postausgang_zu = true;
    return;
  }

  // Liegt noch eine unbestaetigte Zusammenfassung da - vom letzten Mal, als
  // das Telefon nicht zuhoerte -, geht sie jetzt. Und eine liegengebliebene
  // Kurve gleich danach.
  telefon_nachsenden();
  if (!s_hat_wartende && kieselsport_kurve_wartet()) {
    s_kurve_timer = app_timer_register(800, prv_kurve_senden, NULL);
  }
  // Die Einstellungen der Uhr, damit Konfigseite und Kiesel-Helper sie
  // kennen - nach allem, was dringender ist.
  prv_einst_vormerken(2500);
}

void telefon_zuruecksetzen(void) {
  memset(&s_nachfassen, 0, sizeof(s_nachfassen));
  memset(&s_wartet, 0, sizeof(s_wartet));
  memset(&s_liste, 0, sizeof(s_liste));
  memset(&s_pausen, 0, sizeof(s_pausen));
  memset(&s_pausen_laenge, 0, sizeof(s_pausen_laenge));
  memset(&s_ende, 0, sizeof(s_ende));
  memset(&s_hat_wartende, 0, sizeof(s_hat_wartende));
  memset(&s_letzte, 0, sizeof(s_letzte));
  memset(&s_unterwegs_beginn, 0, sizeof(s_unterwegs_beginn));
  memset(&s_postausgang_zu, 0, sizeof(s_postausgang_zu));
  memset(&s_fertig, 0, sizeof(s_fertig));
  memset(&s_stopp_timer, 0, sizeof(s_stopp_timer));
  meldungen_leeren(&s_meldungen);
  memset(&s_zustand_timer, 0, sizeof(s_zustand_timer));
  memset(&s_kurve_unterwegs, 0, sizeof(s_kurve_unterwegs));
  memset(&s_kurve_beginn_unterwegs, 0, sizeof(s_kurve_beginn_unterwegs));
  memset(&s_kurve_ab_unterwegs, 0, sizeof(s_kurve_ab_unterwegs));
  memset(&s_kurve_lesefehler, 0, sizeof(s_kurve_lesefehler));
  memset(&s_kurve_timer, 0, sizeof(s_kurve_timer));
  memset(&s_einst_offen, 0, sizeof(s_einst_offen));
  memset(&s_einst_timer, 0, sizeof(s_einst_timer));
  memset(&s_einst_versuche, 0, sizeof(s_einst_versuche));
}
