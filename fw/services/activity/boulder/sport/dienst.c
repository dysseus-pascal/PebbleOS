/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pbl/services/activity/boulder/sport/kieselsport.h"

#include "abschnitt.h"
#include "art.h"
#include "bahnen.h"
#include "dienst.h"
#include "kurve.h"
#include "pausen.h"
#include "puls.h"
#include "reps.h"
#include "training.h"
#include "wartend.h"

#include "pbl/kernel/mutex.h"
#include "pbl/services/activity/boulder/takt.h"
#include "pbl/services/regular_timer.h"
#include "pbl/services/system_task.h"
#include "pbl/util/testing.h"
#include <pbl/logging/logging.h>

PBL_LOG_MODULE_DECLARE(service_activity, CONFIG_SERVICE_ACTIVITY_LOG_LEVEL);

// Der Dienst: was in Kieselsport der Worker war, jetzt auf Pebbles Workout-Dienst (Weg 2).
//
// PEBBLE FUEHRT, KIESELSPORT FOLGT. Pebbles Workout-Dienst startet, pausiert und stoppt das
// Training, zaehlt Dauer, Schritte und Strecke, schaltet die Auto-Erkennung aus und an, setzt die
// Szene, speichert die Sitzung und zeigt die Meldung. Kieselsport zaehlt Saetze und Bahnen,
// fuehrt Kurve und Pausenliste, brummt bei Zonenwechseln und legt die Zusammenfassung fuers
// Telefon ab - ob die App offen ist oder nicht.
//
// ALLES LAEUFT AUF KERNELBG (Spezifikation 2.5, Regel 1). Der Sekundentakt und das Nachsenden
// kommen vom NewTimer und reihen nur verwerfbar ein (Schritt 1, Regel 1/2): faellt ein Takt
// weg, holt der naechste nach. Die Befehle der App sind nicht verwerfbar, sie duerfen nicht
// verloren gehen. Die App liest nur den Schnappschuss (KsStand), den jeder Takt erneuert.
//
// DIE SPERRE IST EIN BLATT (Regel 2): Unter ihr ruft der Dienst weder Pebbles Workout-Dienst
// noch die Variante noch den HRM-Manager (plattform.h). Jeder Befehl liest darum erst ohne
// Sperre, was Pebble weiss, rechnet dann unter der Sperre und stellt danach ohne Sperre Pebble
// und den Sensor. Dazwischen kann nichts anderes geschehen: Befehle und Takt laufen alle auf
// KernelBG nacheinander; App und KernelMain lesen nur und speisen Sensorwerte ein.

// Fuer die Tests sichtbar (Sperrfolge, T7.2).
PBL_T_STATIC PBL_MUTEX_DEFINE(s_kieselsport_sperre);

void ks_sperren(void) {
  pbl_mutex_lock(&s_kieselsport_sperre, PBL_FOREVER);
}

void ks_entsperren(void) {
  pbl_mutex_unlock(&s_kieselsport_sperre);
}

static bool s_gestartet;  //< kieselsport_init lief (nur KernelBG)

// DIE EINSTELLUNGEN LAEDT, WER SIE ZUERST BRAUCHT (unter der Sperre), nicht erst kieselsport_init.
// Fuer die App kommt init eingereiht auf KernelBG, und KernelBG laeuft unter der App (Regel
// 2.5.5): Bis es dran ist, kann das Telefon schon Einstellungen geschickt haben. Ein Setter
// verglich dann mit den Vorgaben im RAM, hielt die Rueckkehr zum Vorgabewert fuer "unveraendert"
// und schrieb sie nicht - init las danach den alten Wert. Und das Echo nannte die Vorgaben.
static bool s_einstellungen_da;

static void prv_einstellungen_laden(void) {
  if (s_einstellungen_da) {
    return;
  }
  s_einstellungen_da = true;
  training_init();
  reps_laden();
  bahnen_laden();
}

static KsStand s_stand;
static int s_letzte_zone = -1;
static uint8_t s_minute[KS_MINUTE_S];
static uint8_t s_sekunde_der_minute;
static RegularTimerInfo s_takt;

static KsEnde s_ende;
static uint8_t s_ende_nr;

// Die Bestellung der App, bis KernelBG sie abholt.
static Sportart s_bestellt_art;
static uint32_t s_bestellt_beginn;

// V4: WANN PEBBLE STOPPEN SOLL (0 = nichts faellig). Pebbles Meldung legte sich sonst sofort
// ueber den Endschirm mit den Zonen. In diesen 30 s steht Pebbles Training in der Pause; Dauer,
// Schritte und kcal aendern sich nicht. Nur KernelBG liest und schreibt das.
#define STOPP_NACH_S 30
static time_t s_stopp_um;

// JE BEFEHL EINE MESSUNG (Kriterium 7, T7.1): Das Speichern schreibt Zusammenfassung und Kurve in
// den Persist, der Start ruft Pebbles Start mit Szene und Variante - unter einem gemeinsamen Namen
// stuende nur der laengste da, und niemand wuesste, welcher es war.
static BoulderAufgabe s_messung_start = BOULDER_AUFGABE("Kieselsport Start", NULL);
static BoulderAufgabe s_messung_pause = BOULDER_AUFGABE("Kieselsport Pause", NULL);
static BoulderAufgabe s_messung_speichern = BOULDER_AUFGABE("Kieselsport Speichern", NULL);
static BoulderAufgabe s_messung_verwerfen = BOULDER_AUFGABE("Kieselsport Verwerfen", NULL);

// --- Brummen ---
//
// BEIM ZONENWECHSEL HOCH ZWEI KURZE, RUNTER EINER - wer laeuft, soll sie unterscheiden koennen,
// ohne hinzusehen. Beim Kraft einmal ein anderes Muster, wenn die Pause um ist.
static void prv_brummen_hoch(void) {
  static const uint32_t s_muster[] = {60, 80, 60};
  ks_brummen(s_muster, ARRAY_LENGTH(s_muster));
}

static void prv_brummen_runter(void) {
  static const uint32_t s_muster[] = {100};
  ks_brummen(s_muster, ARRAY_LENGTH(s_muster));
}

static void prv_brummen_pause_um(void) {
  static const uint32_t s_muster[] = {80, 100, 80};
  ks_brummen(s_muster, ARRAY_LENGTH(s_muster));
}

// EINE ZONE WECHSELT ERST DREI SCHLAEGE JENSEITS DER GRENZE. Ein Puls, der um einen Schlag um die
// Grenze pendelt, brummte sonst alle paar Sekunden - nachgestellt: 119 Mal in zehn Minuten.
// Angezeigt wird die Zone weiter ohne Abstand (prv_schnappschuss); nur das Brummen wartet, bis
// es eindeutig ist.
#define ZONEN_ABSTAND_BPM 3

static int prv_zone_mit_abstand(uint16_t puls) {
  const int roh = puls_zone(puls);
  if (s_letzte_zone <= 0 || roh == s_letzte_zone) {
    return roh;
  }
  // Hinauf gilt die Zone, die auch drei Schlaege tiefer noch gilt, hinab die, die auch drei
  // hoeher noch gilt. Bis dahin bleibt es die alte.
  if (roh > s_letzte_zone) {
    const int z = puls_zone(puls > ZONEN_ABSTAND_BPM ? (uint16_t)(puls - ZONEN_ABSTAND_BPM) : 0);
    return z > s_letzte_zone ? z : s_letzte_zone;
  }
  const int z = puls_zone((uint16_t)(puls + ZONEN_ABSTAND_BPM));
  return z < s_letzte_zone ? z : s_letzte_zone;
}

static void prv_zonenwechsel(void) {
  // Das ist die eine Stelle, an der die Uhr von sich aus etwas sagt - und der Grund, warum man
  // sie beim Sport ueberhaupt anschaut. Nur ein frischer Wert darf brummen: ein alter wechselt
  // keine Zone.
  const int zone = training_puls_frisch() ? prv_zone_mit_abstand(training_puls()) : 0;
  if (training_zustand() == LaufLaeuft && zone > 0 && s_letzte_zone > 0 &&
      zone != s_letzte_zone) {
    if (zone > s_letzte_zone) {
      prv_brummen_hoch();
    } else {
      prv_brummen_runter();
    }
  }
  if (zone > 0) {
    s_letzte_zone = zone;
  }
  if (reps_brumm_holen()) {
    prv_brummen_pause_um();
  }
}

// AKKU: unter KS_AKKU_SPARSAM Prozent misst der Puls seltener. Beim Start und jede Minute
// nachgesehen - der Stand aendert sich nicht schneller.
static void prv_akku_pruefen(void) {
  bool am_kabel = false;
  const uint8_t prozent = ks_akku_prozent(&am_kabel);
  puls_setze_sparsam(!am_kabel && prozent < KS_AKKU_SPARSAM);
}

// --- Schnappschuss ---

static void prv_schnappschuss(void) {
  const Trainingsstand t = training_stand();
  const uint16_t puls = training_puls();
  const bool frisch = training_puls_frisch();
  s_stand = (KsStand){
    .zustand = training_zustand(),
    .art = training_art(),
    .t = t,
    .puls = puls,
    .frisch = frisch,
    .zone = (uint8_t)(frisch ? puls_zone(puls) : 0),
    .maxpuls = puls_maximum_wirksam(),
    .reps_laufend = reps_laufend(),
    .ruht = reps_ruht(),
    .ruhe_s = reps_ruhe_s(),
    .bahnen_bereit = bahnen_bereit(),
    .sparsam = puls_sparsam(),
    .hrv_anzahl = puls_hrv_anzahl(),
    .ende = s_ende,
    .ende_nr = s_ende_nr,
  };
  memcpy(s_stand.minute, s_minute, sizeof(s_minute));
}

// --- Sensor, ohne Sperre ---

static void prv_sensor_stellen(void) {
  uint16_t takt = 0;
  bool hrv = false;
  ks_sperren();
  const bool faellig = puls_sensor_faellig(&takt, &hrv);
  ks_entsperren();
  if (faellig) {
    ks_puls_sensor(takt, hrv);
  }
}

// --- Pebbles Zeit uebernehmen (unter der Sperre) ---

// So viele aktive Sekunden holt ein Takt einzeln nach (Pebbles Timer laesst bei belegter Sperre
// Ticks aus, ein verworfener Takt kostet eine Sekunde). Mehr ist ein Zeitsprung: Pebbles Dauer
// kommt aus der RTC. Dann springt Kieselsport mit, ohne Kurve und Zaehler fuer die
// uebersprungene Zeit - ein Takt darf nicht eine Stunde Sekunden nachrechnen (Kriterium 7).
#define NACHHOLEN_MAX_S 60

static void prv_eine_sekunde(void) {
  training_tick();
  const uint32_t s = training_sekunde();
  const uint16_t p = training_puls_frisch() ? training_puls() : 0;
  kurve_tick(s, p);
  s_minute[s % KS_MINUTE_S] = p > 255 ? 255 : (uint8_t)p;
}

// EINE UHR FUER DIE AKTIVE ZEIT (Regel 2.5.3): Kieselsport verarbeitet, was Pebbles Dauer seit
// dem letzten Mal gewachsen ist (meist 1 s, mal 0 oder 2), nicht eine feste Sekunde je Takt.
// Steht Pebbles Dauer niedriger (Uhr zurueckgestellt), wartet Kieselsport, bis sie aufholt -
// Kurve, Pausen und Abschnitte bleiben in der Reihe.
static void prv_sekunden_bis(int32_t pebble_s) {
  if (training_zustand() != LaufLaeuft || pebble_s <= (int32_t)training_sekunde()) {
    return;
  }
  const uint32_t ziel = (uint32_t)pebble_s;
  if (ziel - training_sekunde() > NACHHOLEN_MAX_S) {
    PBL_LOG_WRN("Kieselsport: Zeitsprung, %u s ohne Kurve",
                (unsigned)(ziel - training_sekunde() - NACHHOLEN_MAX_S));
    training_springe(ziel - NACHHOLEN_MAX_S);
  }
  while (training_sekunde() < ziel) {
    prv_eine_sekunde();
  }
}

// PEBBLES PAUSE GILT (Regel 2.5.4): Pebble pausiert selbst, sobald das Tracking stoppt (Laden),
// und setzt nicht selbst fort. Kieselsport traegt die Pause ein, und ab dort gilt das Auto-Ende.
// Ein Weiter von anderswo (Pebbles Workout-App) uebernimmt es ebenso.
static void prv_uebernehmen(const KsWorkout *w) {
  training_werte(w);
  const bool pausiert = training_zustand() == LaufPause;
  if (pausiert && !w->pause) {
    training_pause_umschalten();
  }
  prv_sekunden_bis(w->dauer_s);
  if (!pausiert && w->pause) {
    PBL_LOG_INFO("Kieselsport: Pebble hat pausiert");
    training_pause_umschalten();
  }
}

// --- Takt ---

// EIN VERGESSENES TRAINING ENDET VON SELBST. Steht es eine halbe Stunde am Stueck in der Pause,
// ist niemand mehr dabei: es wird gespeichert wie mit der Taste, die aktive Zeit endet beim
// Beginn der Pause. Sonst liefen Sensoren und Sekundentakt die Nacht durch, und das Training
// kaeme erst ans Telefon, wenn jemand die Uhr wieder in die Hand nimmt.
#define AUTO_ENDE_S (30 * 60)
static uint32_t s_pause_s;  //< Takte am Stueck in der Pause

typedef enum {
  AnlassTaste,      //< die App: Pebbles Stopp 30 s spaeter (V4)
  AnlassAutoEnde,   //< 30 min Pause: Pebbles Stopp sofort
  AnlassPebble,     //< Pebble hat das Training schon beendet: nichts mehr zu stoppen
} Anlass;

static void prv_speichern_aus(Anlass anlass);

static void prv_tick(void *daten);
static BoulderAufgabe s_aufgabe_takt = BOULDER_AUFGABE("Kieselsport Takt", prv_tick);

static void prv_takt(void *daten) {
  // NewTimer: nur einreihen, gerechnet wird auf KernelBG.
  boulder_aufgabe_einreihen(&s_aufgabe_takt);
}

static void prv_takt_an(bool an) {
  if (an && !regular_timer_is_scheduled(&s_takt)) {
    s_takt = (RegularTimerInfo){.cb = prv_takt};
    regular_timer_add_seconds_callback(&s_takt);
  } else if (!an && regular_timer_is_scheduled(&s_takt)) {
    regular_timer_remove_callback(&s_takt);
  }
}

// Pebbles faelliger Stopp (V4), ohne Sperre: Sitzung und Meldung.
static void prv_pebble_stoppen(void) {
  s_stopp_um = 0;
  ks_workout_beenden();
}

// Faellig zur Zeit - oder sofort, wenn die Uhr zurueckgestellt wurde: Sonst stuende Pebbles
// Training bis zu einer Stunde (Zeitzone, Abgleich mit dem Telefon) pausiert da, ohne Sitzung.
static bool prv_stopp_faellig(void) {
  const time_t jetzt = ks_jetzt();
  return s_stopp_um && (jetzt >= s_stopp_um || s_stopp_um - jetzt > STOPP_NACH_S);
}

static void prv_tick(void *daten) {
  BOULDER_AUFGABE_LAEUFT(&s_aufgabe_takt);
  if (prv_stopp_faellig()) {
    prv_pebble_stoppen();
  }
  KsWorkout w;
  const bool pebble = ks_workout_lesen(&w);
  ks_sperren();
  if (training_zustand() == LaufAus) {
    // Der Takt lief nur noch fuer Pebbles Stopp.
    if (!s_stopp_um) {
      prv_takt_an(false);
    }
    ks_entsperren();
    return;
  }
  if (!pebble) {
    // PEBBLE HAT DAS TRAINING BEENDET, nicht Kieselsport: ueber Pebbles Workout-App oder ihre
    // Verlassen-Regel. Pebbles Sitzung steht dann schon; Kieselsport legt seine Zusammenfassung
    // dazu, damit das Telefon das Training nicht verliert.
    ks_entsperren();
    PBL_LOG_WRN("Kieselsport: Pebble hat das Training beendet");
    prv_speichern_aus(AnlassPebble);
    return;
  }
  prv_uebernehmen(&w);
  // Gezaehlt wird am Stueck: jedes Weiter faengt von vorn an.
  s_pause_s = training_zustand() == LaufPause ? s_pause_s + 1 : 0;
  if (s_pause_s >= AUTO_ENDE_S) {
    ks_entsperren();
    PBL_LOG_INFO("Kieselsport: %u min Pause, Training endet von selbst",
                 (unsigned)(AUTO_ENDE_S / 60));
    prv_speichern_aus(AnlassAutoEnde);
    return;
  }
  // JEDE MINUTE, AUCH IN DER PAUSE: der Akku, und die Anmeldung am Sensor, die sonst ablaeuft.
  if (++s_sekunde_der_minute >= 60) {
    s_sekunde_der_minute = 0;
    prv_akku_pruefen();
    puls_auffrischen();
  }
  prv_zonenwechsel();
  prv_schnappschuss();
  ks_entsperren();
  prv_sensor_stellen();
}

// --- Nachsenden ---
//
// EIN GESPEICHERTES TRAINING, DAS NIE ANKAM. Die App schickt die Zusammenfassung und wartet
// zwanzig Sekunden auf das Telefon; war es nicht zu erreichen, geht sie zu, und das Training
// laege im Persist, bis jemand die App wieder oeffnet. Also holt der Dienst sie selbst: nach 2,
// 10 und 30 Minuten, dann nicht mehr - ist das Telefon so lange weg, geht es beim naechsten
// Oeffnen.
static const uint8_t NACHSENDEN_MIN[] = {2, 10, 30};
static RegularTimerInfo s_nachsenden_takt;
static uint8_t s_nachsenden_versuch;
static uint16_t s_nachsenden_minuten;

static void prv_nachsenden_aus(void) {
  if (regular_timer_is_scheduled(&s_nachsenden_takt)) {
    regular_timer_remove_callback(&s_nachsenden_takt);
  }
}

static void prv_nachsenden(void *daten);
static BoulderAufgabe s_aufgabe_nachsenden =
    BOULDER_AUFGABE("Kieselsport Nachsenden", prv_nachsenden);

static void prv_nachsenden(void *daten) {
  BOULDER_AUFGABE_LAEUFT(&s_aufgabe_nachsenden);
  ks_sperren();
  // AUCH FUER EINEN STOPP: ohne ihn zeichnete das Telefon weiter auf.
  if (!(wartend_da() || stopp_da()) || s_nachsenden_versuch >= ARRAY_LENGTH(NACHSENDEN_MIN)) {
    prv_nachsenden_aus();
    ks_entsperren();
    return;
  }
  const bool holen = ++s_nachsenden_minuten >= NACHSENDEN_MIN[s_nachsenden_versuch];
  if (holen) {
    s_nachsenden_versuch++;
  }
  ks_entsperren();
  if (holen) {
    ks_app_holen();
  }
}

static void prv_nachsenden_takt(void *daten) {
  boulder_aufgabe_einreihen(&s_aufgabe_nachsenden);
}

static void prv_nachsenden_an(void) {
  s_nachsenden_versuch = 0;
  s_nachsenden_minuten = 0;
  if (!regular_timer_is_scheduled(&s_nachsenden_takt)) {
    s_nachsenden_takt = (RegularTimerInfo){.cb = prv_nachsenden_takt};
    regular_timer_add_minutes_callback(&s_nachsenden_takt);
  }
}

// --- Befehle, auf KernelBG ---

static void prv_beendet(KsEnde ende) {
  // V4: Steht Pebbles Stopp noch aus, laeuft der Takt bis dahin weiter (prv_tick).
  if (!s_stopp_um) {
    prv_takt_an(false);
  }
  s_ende = ende;
  s_ende_nr++;
  prv_schnappschuss();
  ks_persist_offen_halten(false);
}

// Ein verworfenes Training: der Stopp in den Persist, bis das Telefon ihn hat.
static void prv_stopp_merken(void) {
  stopp_merken(training_beginn(), (uint32_t)ks_jetzt(), (uint8_t)training_art());
}

static void prv_starte(void *daten) {
  BOULDER_AUFGABE_LAEUFT(&s_messung_start);
  kieselsport_init();
  // Der Ruhepuls kommt aus der Variante: ohne Sperre lesen (Regel 2.5.2).
  const uint8_t ruhe = ks_ruhepuls();
  ks_sperren();
  if (training_zustand() != LaufAus) {
    ks_entsperren();
    return;
  }
  const Sportart art = s_bestellt_art;
  const uint32_t beginn = s_bestellt_beginn ? s_bestellt_beginn : (uint32_t)ks_jetzt();
  puls_setze_ruhepuls(ruhe);
  // Ein Training braucht die Zonen: spaetestens jetzt ist klar, ob ein alter Wert gilt.
  (void)puls_maximum_gesetzt();
  const uint16_t maxpuls = puls_maximum_wirksam();
  ks_entsperren();

  // REGEL 2.5.7: Laeuft noch Kieselsports eigenes Training aus dem 30-s-Fenster (V4), stoppt es
  // jetzt - mit Sitzung und Meldung -, und dann startet das neue.
  if (s_stopp_um) {
    prv_pebble_stoppen();
  }
  if (!ks_workout_starten(art, maxpuls)) {
    // Ein fremdes Training (Pebbles Workout-App). Die App hat Zustand 1 schon gemeldet und das
    // Telefon zeichnet auf: ein Stopp in den Persist wie beim Verwerfen.
    PBL_LOG_WRN("Kieselsport: Pebble lehnt den Start ab, es laeuft schon ein Training");
    ks_sperren();
    stopp_merken(beginn, (uint32_t)ks_jetzt(), (uint8_t)art);
    s_ende = KsEndeVerworfen;
    s_ende_nr++;
    prv_schnappschuss();
    ks_entsperren();
    prv_nachsenden_an();
    return;
  }
  KsWorkout w;
  ks_workout_lesen(&w);

  ks_sperren();
  // Waehrend des Trainings bleibt die Datei offen: an seinem Ende wird viel geschrieben.
  ks_persist_offen_halten(true);
  // Ein Stopp, der noch wartet, gehoert zu einem frueheren Training: der Start dieses ersetzt
  // ihn auf dem Telefon.
  stopp_vergessen(0);
  s_letzte_zone = -1;
  s_pause_s = 0;
  s_sekunde_der_minute = 0;
  memset(s_minute, 0, sizeof(s_minute));
  prv_akku_pruefen();
  training_starte_ab(art, beginn);
  training_werte(&w);
  kurve_start();
  prv_takt_an(true);
  prv_schnappschuss();
  PBL_LOG_INFO("Kieselsport: Training gestartet, Art %d", (int)art);
  ks_entsperren();
  prv_sensor_stellen();
}

static void prv_pause(void *daten) {
  BOULDER_AUFGABE_LAEUFT(&s_messung_pause);
  ks_sperren();
  const Laufzustand z = training_zustand();
  ks_entsperren();
  if (z == LaufAus) {
    return;
  }
  if (!ks_workout_pause(z == LaufLaeuft)) {
    prv_speichern_aus(AnlassPebble);
    return;
  }
  KsWorkout w;
  ks_workout_lesen(&w);
  ks_sperren();
  // Auch Pause-Weiter-Pause zwischen zwei Takten beginnt eine neue Pause.
  s_pause_s = 0;
  training_werte(&w);
  // Bis zur Sekunde, bei der Pebble angehalten hat; die Pause beginnt dort.
  prv_sekunden_bis(w.dauer_s);
  if (training_zustand() == z) {
    training_pause_umschalten();
  }
  prv_schnappschuss();
  ks_entsperren();
  prv_sensor_stellen();
}

static void prv_speichern_aus(Anlass anlass) {
  // Nicht auf dem Stapel: KernelBG hat keinen grossen.
  static char s_liste[KS_LISTE_MAX];
  static uint8_t s_pausen[KS_PAUSEN_BYTES];
  ks_sperren();
  const Laufzustand z = training_zustand();
  ks_entsperren();
  if (z == LaufAus) {
    return;
  }
  KsWorkout w;
  bool pebble = false;
  if (anlass != AnlassPebble) {
    // DIE DAUER ANHALTEN: Pebbles Stopp kommt erst 30 s spaeter (V4); bis dahin steht Pebbles
    // Training in der Pause, damit Sitzung und Zusammenfassung dieselbe aktive Zeit, dieselben
    // Schritte und dieselben kcal tragen.
    ks_workout_pause(true);
    pebble = ks_workout_lesen(&w);
  }

  ks_sperren();
  if (pebble) {
    training_werte(&w);
    prv_sekunden_bis(w.dauer_s);
  }
  const Trainingsstand t = training_stoppe();
  // EIN TRAINING UNTER EINER MINUTE IST KEINES - es geht nicht ans Telefon und nicht in den
  // Verlauf. Pebble sieht es genauso (workout_service.c, Stopp unter 60 s).
  const bool behalten = t.dauer_s >= 60;
  if (behalten) {
    s_liste[0] = 0;
    if (abschnitt_anzahl() > 0) {
      abschnitt_als_text(s_liste, sizeof(s_liste));
    }
    const size_t pausen = pausen_als_bytes(s_pausen, sizeof(s_pausen));
    wartend_merken(&t, s_liste, s_pausen, pausen, training_ende());
    kurve_merken(t.beginn);
  } else {
    prv_stopp_merken();
  }
  abschnitt_leeren();
  if (behalten && anlass == AnlassTaste) {
    s_stopp_um = ks_jetzt() + STOPP_NACH_S;
  }
  prv_beendet(behalten ? KsEndeGespeichert : KsEndeVerworfen);
  ks_entsperren();

  // OHNE SPERRE ZU PEBBLE. Genau dann eine Sitzung, wenn eine Zusammenfassung im Persist liegt
  // (T2.1). Beide zaehlen Pebbles angehaltene Dauer; unter 60 s verwirft Kieselsport darum
  // ausdruecklich, ohne Meldung und ohne Anstoss ans Datalogging.
  if (anlass != AnlassPebble) {
    if (!behalten) {
      ks_workout_verwerfen();
    } else if (anlass == AnlassAutoEnde) {
      ks_workout_beenden();
    }
  }
  prv_nachsenden_an();
  prv_sensor_stellen();
}

static void prv_speichern(void *daten) {
  BOULDER_AUFGABE_LAEUFT(&s_messung_speichern);
  prv_speichern_aus(AnlassTaste);
}

static void prv_verwerfen(void *daten) {
  BOULDER_AUFGABE_LAEUFT(&s_messung_verwerfen);
  bool verworfen = false;
  ks_sperren();
  if (training_zustand() != LaufAus) {
    prv_stopp_merken();
    verworfen = true;
    training_verwerfen();
    prv_beendet(KsEndeVerworfen);
  }
  ks_entsperren();
  if (verworfen) {
    // F3 = A: Pebble stoppt ohne Sitzung und ohne Meldung.
    ks_workout_verwerfen();
    prv_nachsenden_an();
    prv_sensor_stellen();
  }
}

static void prv_init_cb(void *daten) {
  kieselsport_init();
}

// DIE BESTAETIGTE VELOFAHRT RECHNET MIT DEMSELBEN MAXIMUM (F7): Sie hat keinen Start, an dem
// Kieselsport es setzen koennte. Darum nach dem Laden der Einstellungen und nach jeder
// Aenderung; ohne Sperre, denn kalorien.c gehoert zur Variante (Regel 2.5.2).
static void prv_maxpuls_melden(void) {
  ks_sperren();
  const uint16_t maxpuls = puls_maximum_wirksam();
  ks_entsperren();
  ks_kalorien_maxpuls(maxpuls);
}

// --- Oeffentlich ---

void kieselsport_init(void) {
  // KernelBG, also ohne Wettlauf: Variante und App rufen es beide, gelesen wird einmal je Boot.
  if (s_gestartet) {
    return;
  }
  s_gestartet = true;
  const uint8_t ruhe = ks_ruhepuls();
  ks_sperren();
  prv_einstellungen_laden();
  puls_setze_ruhepuls(ruhe);
  prv_schnappschuss();
  // Nach einem Neustart: was vorher nicht ankam, wird wieder nachgereicht.
  const bool wartet = wartend_da() || stopp_da();
  ks_entsperren();
  prv_maxpuls_melden();
  if (wartet) {
    prv_nachsenden_an();
  }
}

void kieselsport_init_anstossen(void) {
  system_task_add_callback(prv_init_cb, NULL);
}

void kieselsport_starte(Sportart art, uint32_t beginn) {
  ks_sperren();
  s_bestellt_art = art < ArtAnzahl ? art : ArtLaufen;
  s_bestellt_beginn = beginn;
  ks_entsperren();
  system_task_add_callback(prv_starte, NULL);
}

void kieselsport_pause_umschalten(void) {
  system_task_add_callback(prv_pause, NULL);
}

void kieselsport_speichern(void) {
  system_task_add_callback(prv_speichern, NULL);
}

void kieselsport_verwerfen(void) {
  system_task_add_callback(prv_verwerfen, NULL);
}

void kieselsport_stand(KsStand *aus) {
  ks_sperren();
  *aus = s_stand;
  ks_entsperren();
}

bool kieselsport_laeuft(void) {
  ks_sperren();
  const bool laeuft = training_zustand() != LaufAus;
  ks_entsperren();
  return laeuft;
}

const ArtInfo *kieselsport_art_info(Sportart art) {
  return art_info(art);
}

int kieselsport_zone(uint16_t bpm) {
  ks_sperren();
  const int zone = puls_zone(bpm);
  ks_entsperren();
  return zone;
}

uint16_t kieselsport_zonengrenze(int zone) {
  ks_sperren();
  const uint16_t grenze = puls_zonengrenze(zone);
  ks_entsperren();
  return grenze;
}

uint16_t kieselsport_maxpuls(void) {
  ks_sperren();
  prv_einstellungen_laden();
  const uint16_t wert = puls_maximum();
  ks_entsperren();
  return wert;
}

bool kieselsport_maxpuls_gesetzt(void) {
  ks_sperren();
  prv_einstellungen_laden();
  const bool gesetzt = puls_maximum_gesetzt();
  ks_entsperren();
  return gesetzt;
}

void kieselsport_maxpuls_setze(uint16_t schlaege) {
  ks_sperren();
  prv_einstellungen_laden();
  puls_setze_maximum(schlaege);
  prv_schnappschuss();
  ks_entsperren();
  prv_maxpuls_melden();
}

void kieselsport_maxpuls_nicht_setzen(void) {
  ks_sperren();
  prv_einstellungen_laden();
  puls_maximum_nicht_setzen();
  prv_schnappschuss();
  ks_entsperren();
  prv_maxpuls_melden();
}

void kieselsport_maxpuls_alt(uint16_t schlaege) {
  ks_sperren();
  prv_einstellungen_laden();
  puls_maximum_alt(schlaege);
  prv_schnappschuss();
  ks_entsperren();
  prv_maxpuls_melden();
}

uint8_t kieselsport_empfindlichkeit(void) {
  ks_sperren();
  prv_einstellungen_laden();
  const uint8_t wert = reps_empfindlichkeit();
  ks_entsperren();
  return wert;
}

void kieselsport_empfindlichkeit_setze(uint8_t stufe) {
  ks_sperren();
  prv_einstellungen_laden();
  reps_empfindlichkeit_setze(stufe);
  ks_entsperren();
}

uint16_t kieselsport_pausenziel(void) {
  ks_sperren();
  prv_einstellungen_laden();
  const uint16_t wert = reps_pausenziel();
  ks_entsperren();
  return wert;
}

void kieselsport_pausenziel_setze(uint16_t sekunden) {
  ks_sperren();
  prv_einstellungen_laden();
  reps_pausenziel_setze(sekunden);
  ks_entsperren();
}

uint16_t kieselsport_becken(void) {
  ks_sperren();
  prv_einstellungen_laden();
  const uint16_t wert = bahnen_becken();
  ks_entsperren();
  return wert;
}

void kieselsport_becken_setze(uint16_t meter) {
  ks_sperren();
  prv_einstellungen_laden();
  bahnen_becken_setze(meter);
  ks_entsperren();
}

// Zusammenfassung und Kurve liegen nur im Persist, und der hat seine eigene Sperre - fuer das
// Lesen EINES Schluessels reicht sie. NICHT FUER PRUEFEN UND DANN LOESCHEN: das Vergessen nach
// einer Bestaetigung prueft erst, ob noch das bestaetigte Training dasteht, und loescht dann
// mehrere Schluessel. Unterbricht die App (hoehere Prioritaet) dabei das Speichern eines neuen
// Trainings auf KernelBG, stehen dessen erste Schluessel schon da, der gepruefte noch nicht -
// und das Loeschen nimmt sie mit. Ebenso wenig fuer Schluessel, die zusammen gelten
// (kieselsport_kurve_lesen). Das Speichern schreibt unter der Sperre des Dienstes; dieselbe
// nehmen darum diese Aufrufe.

bool kieselsport_wartend_laden(Trainingsstand *t, char *liste, size_t platz) {
  return wartend_laden(t, liste, platz);
}

void kieselsport_wartend_vergessen(uint32_t beginn) {
  ks_sperren();
  wartend_vergessen(beginn);
  ks_entsperren();
}

size_t kieselsport_wartend_pausen(uint8_t *aus, size_t platz) {
  return wartend_pausen(aus, platz);
}

uint32_t kieselsport_wartend_ende(void) {
  return wartend_ende();
}

bool kieselsport_stopp_laden(uint32_t *beginn, uint32_t *zeit, uint8_t *art) {
  return stopp_laden(beginn, zeit, art);
}

void kieselsport_stopp_vergessen(uint32_t beginn) {
  stopp_vergessen(beginn);
}

bool kieselsport_kurve_wartet(void) {
  return kurve_wartet();
}

uint32_t kieselsport_kurve_beginn(void) {
  return kurve_beginn();
}

uint16_t kieselsport_kurve_anzahl(void) {
  return kurve_anzahl();
}

uint16_t kieselsport_kurve_ab(void) {
  return kurve_ab();
}

uint16_t kieselsport_kurve_stueck(uint8_t *aus, uint16_t max) {
  return kurve_stueck(aus, max);
}

// EIN STUECK SAMT KOPF UNTER EINER SPERRE. kurve_merken schreibt erst die Werte, dann Anzahl,
// Beginn und Zeiger. Einzeln gelesen, mitten im Speichern eines neuen Trainings, passten Beginn
// und Zeiger noch zum alten, die Werte schon zum neuen - und das Telefon truege den Puls von B in
// die Kurve von A ein.
uint16_t kieselsport_kurve_lesen(uint32_t *beginn, uint16_t *ab, uint16_t *anzahl, uint8_t *aus,
                                 uint16_t max) {
  ks_sperren();
  *beginn = kurve_beginn();
  *ab = kurve_ab();
  *anzahl = kurve_anzahl();
  const uint16_t n = kurve_stueck(aus, max);
  ks_entsperren();
  return n;
}

bool kieselsport_kurve_bestaetigt(uint32_t beginn, uint16_t ab, uint16_t n) {
  ks_sperren();
  const bool passt = kurve_bestaetigt(beginn, ab, n);
  ks_entsperren();
  return passt;
}

bool kieselsport_kurve_luecke(void) {
  return kurve_luecke();
}

void kieselsport_kurve_vergessen(void) {
  ks_sperren();
  kurve_vergessen();
  ks_entsperren();
}

bool kieselsport_kurve_zeiten(uint32_t sekunden[KS_ZONEN + 1]) {
  // Die Zonen haengen am Maximalpuls - der braucht die Sperre.
  ks_sperren();
  const bool etwas = kurve_zeiten(sekunden);
  ks_entsperren();
  return etwas;
}

#if UNITTEST
// Ein Neustart der Uhr: das RAM des Dienstes ist weg (auch ein ausstehender Stopp, V4), das
// naechste kieselsport_init liest wieder alles aus dem Persist.
// AUCH MITTEN IM TRAINING (AP7, T7.3): Das Training ist dann einfach weg - wie verworfen, aber
// ohne Stopp im Persist (training_verwerfen schreibt nichts). Ebenso der Schnappschuss, die
// Merker der Zeitgeber und der eingereihten Aufgaben: Bliebe ein "steht an" aus der Zeit vor dem
// Neustart stehen, reihte der Takt nie wieder ein - das gaebe es auf der Uhr nicht.
void kieselsport_test_neustart(void) {
  s_gestartet = false;
  s_einstellungen_da = false;
  s_stopp_um = 0;
  if (training_zustand() != LaufAus) {
    training_verwerfen();
  }
  s_stand = (KsStand){0};
  s_letzte_zone = -1;
  memset(s_minute, 0, sizeof(s_minute));
  s_sekunde_der_minute = 0;
  s_takt = (RegularTimerInfo){0};
  s_ende = KsEndeKeins;
  s_ende_nr = 0;
  s_pause_s = 0;
  s_nachsenden_takt = (RegularTimerInfo){0};
  s_nachsenden_versuch = 0;
  s_nachsenden_minuten = 0;
  s_aufgabe_takt.steht_an = false;
  s_aufgabe_nachsenden.steht_an = false;
  ks_plattform_test_neustart();
}
#endif
