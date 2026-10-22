/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "apps/system/kieselsport/atem.h"

#include "applib/ui/animation_timing.h"

// Die Atemuebung als Rechnung (atem.c): Phase, Rest und Runde aus dem Rhythmus und der Zeit, der
// Kreis mit Pebbles ease-in-out (die echte animation_timing.c), wann das naechste Bild faellig
// ist, und das Brummen je Phase.

static Atemrhythmus prv_rhythmus(int ein, int halten, int aus, int leer, int runden) {
  return (Atemrhythmus){.wert = {(uint8_t)ein, (uint8_t)halten, (uint8_t)aus, (uint8_t)leer,
                                 (uint8_t)runden}};
}

static void prv_ist(const Atemrhythmus *r, uint32_t ms, AtemWert phase, int runde,
                    uint32_t rest_ms) {
  const Atemstand s = atem_stand(r, ms);
  cl_assert(!s.fertig);
  cl_assert_equal_i(s.phase, phase);
  cl_assert_equal_i(s.runde, runde);
  cl_assert_equal_i(s.rest_ms, rest_ms);
  cl_assert_equal_i(s.dauer_ms, r->wert[phase] * 1000);
}

void test_kieselsport_atem__initialize(void) {}
void test_kieselsport_atem__cleanup(void) {}

void test_kieselsport_atem__runde_und_gesamt(void) {
  const Atemrhythmus box = prv_rhythmus(4, 4, 4, 4, 10);
  cl_assert_equal_i(atem_runde_ms(&box), 16000);
  cl_assert_equal_i(atem_gesamt_ms(&box), 160000);
  const Atemrhythmus r = prv_rhythmus(4, 7, 8, 0, 3);
  cl_assert_equal_i(atem_runde_ms(&r), 19000);
  cl_assert_equal_i(atem_gesamt_ms(&r), 57000);
  // Die laengste: viermal 60 s, 99 Runden - 6 h 36 min, ohne Ueberlauf.
  const Atemrhythmus lang = prv_rhythmus(60, 60, 60, 60, 99);
  cl_assert_equal_i(atem_gesamt_ms(&lang), 23760000);
}

// Jede Grenze gehoert zur naechsten Phase: 3999 ms ist noch Einatmen, 4000 schon Halten.
void test_kieselsport_atem__phasen_an_ihren_grenzen(void) {
  const Atemrhythmus r = prv_rhythmus(4, 4, 4, 4, 10);
  prv_ist(&r, 0, AtemEin, 1, 4000);
  prv_ist(&r, 1, AtemEin, 1, 3999);
  prv_ist(&r, 3999, AtemEin, 1, 1);
  prv_ist(&r, 4000, AtemHalten, 1, 4000);
  prv_ist(&r, 7999, AtemHalten, 1, 1);
  prv_ist(&r, 8000, AtemAus, 1, 4000);
  prv_ist(&r, 12000, AtemLeer, 1, 4000);
  prv_ist(&r, 15999, AtemLeer, 1, 1);
  prv_ist(&r, 16000, AtemEin, 2, 4000);
  prv_ist(&r, 50000, AtemEin, 4, 2000);
  prv_ist(&r, 58000, AtemAus, 4, 2000);
  prv_ist(&r, 159999, AtemLeer, 10, 1);
}

// Ein Halten von 0 s kommt nie vor: nach dem Einatmen gleich das Ausatmen, nach dem Ausatmen die
// naechste Runde - auch auf jeder einzelnen Millisekunde.
void test_kieselsport_atem__null_halten_faellt_weg(void) {
  const Atemrhythmus r = prv_rhythmus(3, 0, 5, 0, 4);
  prv_ist(&r, 2999, AtemEin, 1, 1);
  prv_ist(&r, 3000, AtemAus, 1, 5000);
  prv_ist(&r, 7999, AtemAus, 1, 1);
  prv_ist(&r, 8000, AtemEin, 2, 3000);
  for (uint32_t ms = 0; ms < atem_gesamt_ms(&r); ms++) {
    const Atemstand s = atem_stand(&r, ms);
    cl_assert(s.phase == AtemEin || s.phase == AtemAus);
    cl_assert(s.rest_ms >= 1 && s.rest_ms <= s.dauer_ms);
  }
  // Nur das leere Halten faellt weg, das volle bleibt.
  const Atemrhythmus voll = prv_rhythmus(2, 1, 2, 0, 1);
  prv_ist(&voll, 2000, AtemHalten, 1, 1000);
  prv_ist(&voll, 3000, AtemAus, 1, 2000);
  cl_assert(atem_stand(&voll, 5000).fertig);
}

// Die Runden zaehlen von 1, und nach der letzten ist die Uebung fertig - genau mit der letzten
// Millisekunde der letzten Phase.
void test_kieselsport_atem__runden_und_ende(void) {
  const Atemrhythmus r = prv_rhythmus(4, 4, 4, 4, 10);
  for (int runde = 1; runde <= 10; runde++) {
    const uint32_t beginn = (uint32_t)(runde - 1) * 16000;
    cl_assert_equal_i(atem_stand(&r, beginn).runde, runde);
    cl_assert_equal_i(atem_stand(&r, beginn + 15999).runde, runde);
  }
  cl_assert(!atem_stand(&r, 159999).fertig);
  const Atemstand ende = atem_stand(&r, 160000);
  cl_assert(ende.fertig);
  cl_assert_equal_i(ende.runde, 10);
  cl_assert_equal_i(ende.phase, AtemLeer);
  cl_assert_equal_i(ende.rest_ms, 0);
  cl_assert(atem_stand(&r, 4000000).fertig);
  // Ohne leeres Halten endet die letzte Runde mit dem Ausatmen.
  const Atemrhythmus ohne = prv_rhythmus(4, 4, 6, 0, 2);
  cl_assert_equal_i(atem_stand(&ohne, 28000).phase, AtemAus);
  cl_assert(atem_stand(&ohne, 28000).fertig);
  // Ein Rhythmus ohne Zeit oder ohne Runden ist sofort zu Ende, statt durch 0 zu teilen.
  const Atemrhythmus leer = prv_rhythmus(0, 0, 0, 0, 5);
  cl_assert(atem_stand(&leer, 0).fertig);
  const Atemrhythmus keine = prv_rhythmus(4, 4, 4, 4, 0);
  cl_assert(atem_stand(&keine, 0).fertig);
}

// Wie ein Countdown: 4, 3, 2, 1 - die Zahl springt, wenn eine ganze Sekunde erreicht ist.
void test_kieselsport_atem__sekunden_wie_ein_countdown(void) {
  const Atemrhythmus r = prv_rhythmus(4, 4, 4, 4, 10);
  Atemstand s = atem_stand(&r, 0);
  cl_assert_equal_i(atem_rest_s(&s), 4);
  s = atem_stand(&r, 999);
  cl_assert_equal_i(atem_rest_s(&s), 4);
  s = atem_stand(&r, 1000);
  cl_assert_equal_i(atem_rest_s(&s), 3);
  s = atem_stand(&r, 3999);
  cl_assert_equal_i(atem_rest_s(&s), 1);
  s = atem_stand(&r, 4000);
  cl_assert_equal_i(atem_rest_s(&s), 4);
  s = atem_stand(&r, 160000);
  cl_assert_equal_i(atem_rest_s(&s), 0);
}

// Der Kreis: einatmen von r_min nach r_max mit Pebbles ease-in-out (am Anfang und am Ende langsam,
// in der Mitte die Haelfte), halten oben, ausatmen zurueck, leer unten.
void test_kieselsport_atem__radius(void) {
  const Atemrhythmus r = prv_rhythmus(4, 4, 4, 4, 10);
  const int16_t r_min = 20, r_max = 60;
  Atemstand s = atem_stand(&r, 0);
  cl_assert_equal_i(atem_radius(&s, r_min, r_max), 20);
  s = atem_stand(&r, 2000);
  cl_assert_equal_i(atem_radius(&s, r_min, r_max), 40);
  // Ein Viertel der Zeit ist ein Achtel des Wegs (ease-in): 25 statt 30 bei linear.
  s = atem_stand(&r, 1000);
  cl_assert_equal_i(atem_radius(&s, r_min, r_max), 25);
  s = atem_stand(&r, 3000);
  cl_assert_equal_i(atem_radius(&s, r_min, r_max), 55);
  s = atem_stand(&r, 3999);
  cl_assert_equal_i(atem_radius(&s, r_min, r_max), 60);
  s = atem_stand(&r, 4000);
  cl_assert_equal_i(atem_radius(&s, r_min, r_max), 60);
  s = atem_stand(&r, 7999);
  cl_assert_equal_i(atem_radius(&s, r_min, r_max), 60);
  s = atem_stand(&r, 8000);
  cl_assert_equal_i(atem_radius(&s, r_min, r_max), 60);
  s = atem_stand(&r, 9000);
  cl_assert_equal_i(atem_radius(&s, r_min, r_max), 55);
  s = atem_stand(&r, 10000);
  cl_assert_equal_i(atem_radius(&s, r_min, r_max), 40);
  s = atem_stand(&r, 12000);
  cl_assert_equal_i(atem_radius(&s, r_min, r_max), 20);
  s = atem_stand(&r, 160000);
  cl_assert_equal_i(atem_radius(&s, r_min, r_max), 20);
  // Stetig: von Millisekunde zu Millisekunde springt der Kreis nie um mehr als einen Punkt.
  int16_t vorher = r_min;
  for (uint32_t ms = 0; ms < 16000; ms++) {
    s = atem_stand(&r, ms);
    const int16_t jetzt = atem_radius(&s, r_min, r_max);
    cl_assert(jetzt - vorher <= 1 && vorher - jetzt <= 1);
    cl_assert(jetzt >= r_min && jetzt <= r_max);
    vorher = jetzt;
  }
}

// Im Atmen gut 15 Bilder je Sekunde, beim Halten eines je Sekunde - jeweils so, dass die Zahl und
// der Wechsel der Phase genau auf ihrer Millisekunde gezeichnet werden.
void test_kieselsport_atem__naechstes_bild(void) {
  const Atemrhythmus r = prv_rhythmus(4, 4, 4, 4, 10);
  Atemstand s = atem_stand(&r, 0);
  cl_assert_equal_i(atem_naechstes_bild_ms(&s), 66);
  s = atem_stand(&r, 950);
  cl_assert_equal_i(atem_naechstes_bild_ms(&s), 50);
  s = atem_stand(&r, 3990);
  cl_assert_equal_i(atem_naechstes_bild_ms(&s), 10);
  s = atem_stand(&r, 4000);
  cl_assert_equal_i(atem_naechstes_bild_ms(&s), 1000);
  s = atem_stand(&r, 4300);
  cl_assert_equal_i(atem_naechstes_bild_ms(&s), 700);
  s = atem_stand(&r, 9000);
  cl_assert_equal_i(atem_naechstes_bild_ms(&s), 66);
  s = atem_stand(&r, 12000);
  cl_assert_equal_i(atem_naechstes_bild_ms(&s), 1000);
  s = atem_stand(&r, 160000);
  cl_assert_equal_i(atem_naechstes_bild_ms(&s), 0);
  // Nie ueber den Wechsel hinaus: von jeder Millisekunde aus landet das naechste Bild spaetestens
  // auf dem Beginn der naechsten Phase.
  for (uint32_t ms = 0; ms < 32000; ms++) {
    s = atem_stand(&r, ms);
    const uint32_t bild = atem_naechstes_bild_ms(&s);
    cl_assert(bild >= 1 && bild <= s.rest_ms);
  }
}

// Mit geschlossenen Augen: einatmen ein kurzes (100 ms), halten zwei sehr kurze (50, Pause 100,
// 50) - voll wie leer -, ausatmen ein langes (300 ms). Das Ende klingt wie keine Phase.
void test_kieselsport_atem__brummen_je_phase(void) {
  VibePattern m = atem_muster(AtemEin);
  cl_assert_equal_i(m.num_segments, 1);
  cl_assert_equal_i(m.durations[0], 100);
  m = atem_muster(AtemHalten);
  cl_assert_equal_i(m.num_segments, 3);
  cl_assert_equal_i(m.durations[0], 50);
  cl_assert_equal_i(m.durations[1], 100);
  cl_assert_equal_i(m.durations[2], 50);
  const VibePattern leer = atem_muster(AtemLeer);
  cl_assert(leer.durations == m.durations && leer.num_segments == m.num_segments);
  m = atem_muster(AtemAus);
  cl_assert_equal_i(m.num_segments, 1);
  cl_assert_equal_i(m.durations[0], 300);
  const VibePattern ende = atem_muster_ende();
  cl_assert(ende.num_segments == 5);
  cl_assert_equal_i(ende.durations[4], 500);
}
