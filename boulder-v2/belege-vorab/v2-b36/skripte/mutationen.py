#!/usr/bin/env python3
"""b36: Mutationen der Atemuebung. Je Mutation die Quelle aendern, das Testziel bauen, die genannten
Tests laufen lassen - rot erwartet -, die Quelle zurueck. Am Ende alles wieder gebaut und gruen.

Aufruf: mutationen.py <ausgabe.txt>
"""
import pathlib
import subprocess
import sys

R = pathlib.Path('/home/user/pebbleos-v2')
APP = 'fw/apps/system/kieselsport/'
SPORT = 'fw/services/activity/boulder/sport/'

# (Kennung, Datei, alt, neu, Testziel, [Tests per -t] oder None fuer alle, Beschreibung)
M = [
    ('A1', APP + 'einstellungen.c', 'art > ArtAnzahl) return;', 'art > 7) return;',
     'test_kieselsport_einstellungen', ['pin_fuer_jede_art'], 'Pin-Grenze wieder fest 7'),
    ('R1', SPORT + 'rhythmus.c', 'return wert >= 1 && wert <= PHASE_MAX_S;',
     'return wert >= 0 && wert <= PHASE_MAX_S;', 'test_kieselsport',
     ['atem_grenzen_und_nur_was_sich_aendert'], 'Ein-/Ausatmen 0 s erlaubt'),
    ('R2', SPORT + 'rhythmus.c', 'return wert >= 1 && wert <= RUNDEN_MAX;',
     'return wert >= 1 && wert <= 255;', 'test_kieselsport',
     ['atem_grenzen_und_nur_was_sich_aendert'], 'Runden ohne Obergrenze 99'),
    ('R3', SPORT + 'rhythmus.c', 'if (!prv_gueltig(welcher, wert) || s_rhythmus.wert[welcher] == wert) {',
     'if (!prv_gueltig(welcher, wert)) {', 'test_kieselsport',
     ['atem_grenzen_und_nur_was_sich_aendert'], 'schreibt auch Unveraendertes'),
    ('R4', SPORT + 'rhythmus.c', '    if (prv_gueltig((AtemWert)i, wert)) {\n      s_rhythmus',
     '    if (true) {\n      s_rhythmus', 'test_kieselsport', ['atem_unsinn_im_persist'],
     'Unsinn aus dem Persist gilt'),
    ('R5', SPORT + 'dienst.c', '  prv_einstellungen_laden();\n  rhythmus_setze(welcher, wert);',
     '  rhythmus_setze(welcher, wert);', 'test_kieselsport', ['atem_vor_init'],
     'Setzen ohne vorher zu laden (AP3)'),
    ('R6', SPORT + 'dienst.c', '  bahnen_laden();\n  rhythmus_laden();', '  bahnen_laden();',
     'test_kieselsport', ['atem_rhythmus_im_persist'], 'Rhythmus nie aus dem Persist geladen'),
    ('R7', SPORT + 'rhythmus.c', '#define VORGABE     {.wert = {4, 4, 4, 4, 10}}',
     '#define VORGABE     {.wert = {4, 4, 4, 4, 12}}', 'test_kieselsport',
     ['atem_rhythmus_im_persist'], 'andere Vorgabe'),
    ('E1', APP + 'einstellungen.c', '    dict_write_int32(out, MESSAGE_KEY_ATEM_EIN + i, atem.wert[i]);',
     '    dict_write_int32(out, MESSAGE_KEY_ATEM_EIN + i, atem.wert[AtemEin]);',
     'test_kieselsport_einstellungen', ['golden'], 'Meldung traegt falsche Werte'),
    ('E2', APP + 'einstellungen.c', '  for (int i = 0; i < AtemWerte; i++) {\n    dict_write_int32',
     '  for (int i = 0; i < AtemRunden; i++) {\n    dict_write_int32',
     'test_kieselsport_wechsel', ['einstellungen_wie_b25'], 'Meldung ohne Runden'),
    ('T1', APP + 'telefon.c', '      atem = true;\n', '\n', 'test_kieselsport_telefon',
     ['atem_ein_schluessel_allein'], 'kein Echo nach dem Rhythmus'),
    ('T2', APP + 'telefon.c', 'einstellungen_atem((AtemWert)i, prv_zahl(t));',
     'einstellungen_atem((AtemWert)i, t->value->int32);', 'test_kieselsport_telefon',
     ['atem_rhythmus_vom_telefon'], 'nur int32 statt int/uint/Text'),
    ('D1', SPORT + 'art.c', '  { false,    false,   false, false,  true  },   // Atmen',
     '  { false,    false,   false, false,  false },   // Atmen', 'test_kieselsport',
     ['atem_misst_wie_yoga'], 'Atmen ohne HRV'),
    ('D2', SPORT + 'training.c', '  if (info->hrv) t.hrv_ms = puls_hrv_rmssd();',
     '  if (s_art == ArtYoga) t.hrv_ms = puls_hrv_rmssd();', 'test_kieselsport',
     ['atem_misst_wie_yoga'], 'HRV nur fuer Yoga in die Zusammenfassung'),
    ('D3', SPORT + 'plattform.c', 'if (art == ArtYoga || art == ArtSchwimmen || art == ArtAtem) {',
     'if (art == ArtYoga || art == ArtSchwimmen) {', 'test_kieselsport', ['atem_misst_wie_yoga'],
     'Szene hohe Belastung beim Atmen'),
    ('D4', SPORT + 'dienst.c', 'training_art() != ArtAtem && zone > 0', 'zone > 0',
     'test_kieselsport', ['atem_brummt_keine_zonen'], 'Zonen brummen beim Atmen'),
    ('S1', APP + 'symbole.c', 's_yoga, s_schwimmen, s_atem,', 's_yoga, s_schwimmen,',
     'test_kieselsport_arten', None, 'kein Symbol fuer Atmen'),
    ('S1b', APP + 'symbole.c', 'static const int8_t s_atem[] = {\n  LZ, 2,', 'static const int8_t s_atem[] = {\n  ENDE, 2,',
     'test_kieselsport_arten', None, 'Symbol fuer Atmen leer'),
    ('S2', APP + 'art_text.c', 'STR_ART_SCHWIMMEN, STR_ART_ATEM,', 'STR_ART_SCHWIMMEN,',
     'test_kieselsport_arten', None, 'kein Name fuer Atmen'),
    ('S3', APP + 'thema.c', '    case ArtAtem:      return GColorCobaltBlue;\n', '',
     'test_kieselsport_arten', None, 'keine Farbe fuer Atmen'),
    ('S4', APP + 'thema.c', 'case ArtAtem:      return GColorCobaltBlue;', 'case ArtAtem:      return GColorTiffanyBlue;',
     'test_kieselsport_arten', None, 'Farbe wie Schwimmen'),
    ('P1', APP + 'atem.c', '  while (in_runde >= prv_phase_ms(r, p)) {', '  while (in_runde > prv_phase_ms(r, p)) {',
     'test_kieselsport_atem', None, 'Phase von 0 s bleibt stehen'),
    ('P2', APP + 'atem.c', '.runde = (uint8_t)(ms / runde_ms + 1),', '.runde = (uint8_t)(ms / runde_ms),',
     'test_kieselsport_atem', None, 'Runden ab 0'),
    ('P3', APP + 'atem.c', 'ms >= runde_ms * runden) {', 'ms > runde_ms * runden) {',
     'test_kieselsport_atem', None, 'Ende eine ms zu spaet'),
    ('P4', APP + 'atem.c', '  return (s->rest_ms + 999) / 1000;', '  return s->rest_ms / 1000;',
     'test_kieselsport_atem', None, 'Sekunden abgerundet'),
    ('P5', APP + 'atem.c', '  const int32_t kurve = animation_timing_curve(t, AnimationCurveEaseInOut);',
     '  const int32_t kurve = t;', 'test_kieselsport_atem', None, 'Kreis linear statt ease-in-out'),
    ('P6', APP + 'atem.c', 'return (int16_t)(s->phase == AtemEin ? r_min + weg : r_max - weg);',
     'return (int16_t)(r_min + weg);', 'test_kieselsport_atem', None, 'Ausatmen waechst'),
    ('P7', APP + 'atem.c', '  if (s->phase == AtemHalten) {\n    return r_max;', '  if (s->phase == AtemHalten) {\n    return r_min;',
     'test_kieselsport_atem', None, 'volles Halten klein'),
    ('P8', APP + 'atem.c', '#define BILD_MS 66', '#define BILD_MS 100', 'test_kieselsport_atem', None,
     '10 statt 15 Bilder je Sekunde'),
    ('P9', APP + 'atem.c', "  if ((s->phase == AtemEin || s->phase == AtemAus) && ms > BILD_MS) {",
     "  if (ms > BILD_MS) {", 'test_kieselsport_atem', None, 'auch beim Halten 15 Bilder'),
    ('P10', APP + 'atem.c', 'static const uint32_t s_aus[] = {300};', 'static const uint32_t s_aus[] = {100};',
     'test_kieselsport_atem', None, 'Ausatmen brummt wie Einatmen'),
    ('P11', APP + 'atem.c', 'static const uint32_t s_halten[] = {50, 100, 50};',
     'static const uint32_t s_halten[] = {100};', 'test_kieselsport_atem', None, 'Halten brummt wie Einatmen'),
    ('L1', APP + 'lauf_window.c', '  if (melden) s_atem_gemeldet.runde = 0;\n', '  s_atem_gemeldet.runde = 0;\n',
     'test_kieselsport_lauf', ['atem_brummt_jede_phase'], 'erstes Einatmen brummt nach dem Los'),
    ('L2', APP + 'lauf_window.c', '  if (melden) s_atem_gemeldet.runde = 0;\n', '\n',
     'test_kieselsport_lauf', ['atem_ohne_countdown_brummt_das_einatmen'], 'ohne Countdown stumm'),
    ('L3', APP + 'lauf_window.c', '    vibes_enqueue_custom_pattern(atem_muster(st.phase));\n    s_atem_gemeldet = st;',
     '    s_atem_gemeldet = st;', 'test_kieselsport_lauf', ['atem_brummt_jede_phase'], 'Phasen brummen nicht'),
    ('L4', APP + 'lauf_window.c', '    vibes_enqueue_custom_pattern(atem_muster_ende());\n    prv_atem_beenden();',
     '    prv_atem_beenden();', 'test_kieselsport_lauf', ['atem_ohne_halten'], 'Ende brummt nicht'),
    ('L5', APP + 'lauf_window.c', '  s_atem_ms = prv_atem_jetzt();\n  s_atem_anker = prv_uhr_ms();\n  s_atem_laeuft = laufen;',
     '  s_atem_anker = prv_uhr_ms();\n  s_atem_laeuft = laufen;', 'test_kieselsport_lauf',
     ['atem_pause_haelt_den_takt_an'], 'Pause friert den Stand nicht ein'),
    ('L6', APP + 'lauf_window.c', '  if (prv_uhr_ms() >= s_atem_taste_bis) prv_atem_folgen(s.zustand);',
     '  prv_atem_folgen(s.zustand);', 'test_kieselsport_lauf', ['atem_alter_stand_nach_der_taste'],
     'alter Stand nach der Taste gilt'),
    ('L7', APP + 'lauf_window.c', '    prv_atem_beginnen(k->t.dauer_s * 1000, k->zustand == LaufLaeuft, false);',
     '    prv_atem_beginnen(0, k->zustand == LaufLaeuft, false);', 'test_kieselsport_lauf',
     ['atem_neu_geoeffnet'], 'neu geoeffnet bei 0 statt bei der Zeit des Dienstes'),
    ('L8', APP + 'lauf_window.c', '  if (s_atem_gemeldet.fertig) {\n    // Neu geoeffnet',
     '  if (false) {\n    // Neu geoeffnet', 'test_kieselsport_lauf', ['atem_beim_oeffnen_schon_um'],
     'nach dem Ende geoeffnet laeuft weiter'),
    ('L9', APP + 'lauf_window.c', 'const bool gezaehlt = (uint64_t)s.sekunden * 1000 >= atem_gesamt_ms(&s_atem);',
     'const bool gezaehlt = true;', 'test_kieselsport_lauf', ['atem_ende_wie_pause_und_speichern'],
     'speichert vor der ganzen Zeit'),
    ('L10', APP + 'lauf_window.c', '      prv_uhr_ms() - s_atem_ende_seit < ATEM_ENDE_WARTEN_MS) {',
     '      true) {', 'test_kieselsport_lauf', ['atem_ende_wartet_nicht_ewig'], 'wartet ewig auf den Dienst'),
    ('L11', APP + 'lauf_window.c', '  if (s.zustand == LaufLaeuft) prv_pause_umschalten();\n  kieselsport_speichern();',
     '  kieselsport_speichern();', 'test_kieselsport_lauf', ['atem_ende_wie_pause_und_speichern'],
     'Ende ohne Pause'),
    ('L12', APP + 'lauf_window.c', 'app_timer_register(sichtbar ? atem_naechstes_bild_ms(&st) : st.rest_ms,',
     'app_timer_register(st.rest_ms,', 'test_kieselsport_lauf', ['atem_bilder'], 'keine Bilder im Atmen'),
    ('L13', APP + 'lauf_window.c', 'app_timer_register(sichtbar ? atem_naechstes_bild_ms(&st) : st.rest_ms,',
     'app_timer_register(atem_naechstes_bild_ms(&st),', 'test_kieselsport_lauf', ['atem_auf_anderer_seite'],
     'zeichnet auch auf anderer Seite'),
    ('L14', APP + 'lauf_window.c', '    // Zurueck auf der Atemseite braucht der Kreis gleich wieder seine Bilder.\n    prv_atem_neu_stellen();\n',
     '', 'test_kieselsport_lauf', ['atem_auf_anderer_seite'], 'zurueck auf der Atemseite ohne Bilder'),
    ('L15', APP + 'lauf_window.c', '    prv_atem_beenden();\n    return;\n  }\n  if (st.phase',
     '    return;\n  }\n  if (st.phase', 'test_kieselsport_lauf',
     ['atem_ohne_halten', 'atem_ende_wie_pause_und_speichern'], 'Ende ohne Speichern'),
]


def lauf(befehl, **kw):
    return subprocess.run(befehl, cwd=R, capture_output=True, text=True, **kw)


def runme(ziel):
    for p in (R / 'build-test').rglob('runme'):
        if p.parent.name == ziel:
            return p
    raise SystemExit(f'runme fuer {ziel} fehlt')


aus = open(sys.argv[1], 'w')
nur = set(sys.argv[2:])
if nur:
    M = [m for m in M if m[0] in nur]
rot = 0
for kennung, datei, alt, neu, ziel, tests, was in M:
    pfad = R / datei
    original = pfad.read_text()
    if original.count(alt) != 1:
        zeile = f'{kennung}: Muster nicht genau einmal in {datei}'
        print(zeile, flush=True)
        aus.write(zeile + '\n')
        continue
    try:
        pfad.write_text(original.replace(alt, neu))
        bau = lauf(['ninja', '-C', 'build-test', ziel])
        if bau.returncode != 0:
            ergebnis = 'rot (Bau)'
        else:
            fehl = []
            for t in (tests or [None]):
                befehl = [str(runme(ziel))] + ([f'-t{t}'] if t else [])
                r = subprocess.run(befehl, cwd=runme(ziel).parent, capture_output=True, text=True,
                                   timeout=900)
                if r.returncode != 0:
                    fehl.append(t or 'alle')
            ergebnis = f'rot ({", ".join(fehl)})' if fehl else 'GRUEN'
    finally:
        pfad.write_text(original)
    rot += ergebnis.startswith('rot')
    zeile = f'{kennung:4s} {ergebnis:45s} {was} [{datei}, {ziel}]'
    print(zeile, flush=True)
    aus.write(zeile + '\n')
zeile = f'{rot}/{len(M)} rot'
print(zeile)
aus.write(zeile + '\n')
# Alles zurueck gebaut und gruen.
ziele = sorted({m[4] for m in M})
bau = lauf(['ninja', '-C', 'build-test'] + ziele)
aus.write(f'Rueckbau rc={bau.returncode}\n')
for z in ziele:
    r = subprocess.run([str(runme(z))], cwd=runme(z).parent, capture_output=True, text=True, timeout=900)
    aus.write(f'{z} nach dem Rueckbau rc={r.returncode}\n')
    print(f'{z} nach dem Rueckbau rc={r.returncode}')
aus.close()
