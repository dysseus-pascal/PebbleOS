#!/usr/bin/env python3
"""b40: Mutationen des Schlafmodus von Hand (F53-F56): Fenster, Ende, Bit, Ablage, Laden, S8, die
Health-App und der Schalter fuer Quick Launch. Wie b39s mutationen.py: je Mutation die Quelle
aendern, das Testziel bauen, die genannten Tests laufen lassen - rot erwartet -, die Quelle zurueck.
Am Ende alles wieder gebaut und gruen.

Aufruf: mutationen.py <ausgabe.txt> [Kennung ...]
"""
import pathlib
import subprocess
import sys

R = pathlib.Path('/home/user/pebbleos-v2')
SM = 'fw/services/activity/boulder/schlafmodus.c'
DT = 'fw/services/activity/boulder/dauerteil.c'
NA = 'fw/services/activity/boulder/nacht.c'
AU = 'fw/services/activity/boulder/ausgabe.c'
SP = 'fw/services/activity/boulder/speicher.c'
SS = 'fw/services/activity/boulder/schnittstelle.c'
BH = 'fw/apps/system/boulder_health/boulder_health.c'
IN = 'fw/apps/system/boulder_health/inhalt.c'
SW = 'fw/apps/system/boulder_schlaf/boulder_schlaf.c'
ZY = 'test_boulder_zyklen'
SCH = 'test_boulder_schnittstelle'
APP = 'test_bh_app'
KA = 'test_bh_karten_emery'
TS = 'test_boulder_schlafschalter'
ALLE = ['schlafmodus_fenster', 'schlafmodus_am_tag', 'schlafmodus_am_abend',
        'schlafmodus_neustart_im_schlaf', 'schlafmodus_ohne_schlaf', 'schlafmodus_gleich_beendet',
        'schlafmodus_15_stunden', 'schlafmodus_vor_dem_laden', 'schlafmodus_nachtruhe_am_tag',
        'schlafmodus_erste_minute', 'schlafmodus_aufgewacht_sofort']
APP_T = ['schlafmodus_starten', 'schlafmodus_laeuft', 'schlafmodus_endete_eben']

# (Kennung, Datei, alt, neu, Testziel, [Tests per -t] oder None fuer alle, Beschreibung)
M = [
    ('S1', SM, '  if (s_zustand.aktiv) {\n    return start;\n  }',
     '  if (s_zustand.aktiv) {\n    return regulaer;\n  }', ZY, ALLE, 'laufender Schlafmodus ohne eigenes Fenster'),
    ('S2', SM, '  if (regulaer >= (time_t)s_zustand.ende || utc - start >= SCHLAFMODUS_FENSTER_MAX_S) {',
     '  if (utc - start >= SCHLAFMODUS_FENSTER_MAX_S) {', ZY, ALLE,
     'nach dem Ende kein Fenster ab 20 Uhr bis 24 h'),
    ('S3', SM, '  return s_zustand.aktiv || eingeschlafen < (time_t)s_zustand.ende;',
     '  return s_zustand.aktiv;', ZY, ALLE, 'Bit nur, solange er laeuft'),
    ('S4', SM, '      ende - (time_t)s_zustand.start < SCHLAFMODUS_OHNE_SCHLAF_MIN * PBL_SEC_PER_MIN) {',
     '      ende - (time_t)s_zustand.start < 2 * SCHLAFMODUS_OHNE_SCHLAF_MIN * PBL_SEC_PER_MIN) {',
     ZY, ALLE, 'Ende ohne Schlaf erst nach vier Stunden'),
    ('S5', SM, '  s_zustand.geschlafen |= schlaeft;\n', '', ZY, ALLE, 'Schlaf seit dem Start vergessen'),
    ('S6', SM, '  if (!s_zustand.aktiv || fenster != (time_t)s_zustand.start) {',
     '  if (!s_zustand.aktiv) {', ZY, ALLE, 'jede endgueltige Nacht beendet ihn, auch die alte'),
    ('S7', SM, '  if (pebble_task_get_current() != PebbleTask_App) {', '  if (true) {', SCH,
     ['schlafmodus_aus_der_app'], 'aus der App verwerfbar eingereiht'),
    ('S8', SM, '  if (ein->fassung != SCHLAFMODUS_FASSUNG) {', '  if (ein->fassung == SCHLAFMODUS_FASSUNG) {',
     ZY, ALLE, 'Zustand aus der Datei nie geladen'),
    ('D1', DT, '  return boulder_schlafmodus_fenster(utc, prv_fenster_regulaer(utc));',
     '  return prv_fenster_regulaer(utc);', ZY, ALLE, 'Fenster nie von Hand'),
    ('D2', DT, '  if (boulder_schlafmodus_minute(minute->utc, boulder_live_schlaeft())) {',
     '  if (boulder_schlafmodus_minute(minute->utc, false)) {', ZY, ALLE,
     'Schlaf des Live-Automaten zaehlt nicht'),
    ('D3', DT, '  const int naechste = boulder_schlafmodus_aktiv()\n',
     '  const int naechste = false\n', ZY, ['schlafmodus_nachtruhe_am_tag'],
     'Nachtruhe der Messung nur ab 20 Uhr'),
    ('D4', DT, '    boulder_nacht_schliessen();\n', '', ZY, ALLE, 'Start schliesst das alte Fenster nicht'),
    ('D5', DT, '    boulder_nacht_von_hand_beendet();\n', '', ZY, ALLE, '"Aufgewacht" schliesst die Nacht nicht'),
    ('D6', DT, '  if (!s_gestartet || s_laden_offen) {\n    s_schlafmodus_nach_laden = an ? 1 : -1;\n    return;\n  }\n',
     '', ZY, ALLE, 'Wunsch vor dem Laden nicht aufgeschoben'),
    ('D7', DT, '  if (beginn == boulder_nacht_fenster() || beginn == prv_fenster_regulaer(jetzt)) {\n    return;\n  }',
     '  if (true) {\n    return;\n  }', ZY, ALLE, 'Eingang nach dem Neustart nicht nachgefuellt'),
    ('N1', NA, '      e->gefunden && (s_fensterende || s_von_hand_beendet || wach >= NACHT_WACH_FERTIG);',
     '      e->gefunden && (s_fensterende || wach >= NACHT_WACH_FERTIG);', ZY, ALLE,
     '"Aufgewacht" macht die Nacht nicht fertig'),
    ('N2', NA, '      s_fensterende || (e->gefunden && (s_von_hand_beendet || wach >= NACHT_WACH_ENDGUELTIG));',
     '      s_fensterende || (e->gefunden && wach >= NACHT_WACH_ENDGUELTIG);', ZY, ALLE,
     '"Aufgewacht" macht die Nacht nicht endgueltig'),
    ('N3', NA, '  if (boulder_schlafmodus_nacht_endgueltig(s_beginn, ende)) {',
     '  if (false && boulder_schlafmodus_nacht_endgueltig(s_beginn, ende)) {', ZY, ALLE,
     'endgueltige Nacht beendet ihn nicht'),
    ('A1', AU, '                            ? NACHT_DLS_FLAG_MANUELL\n',
     '                            ? 0\n', ZY, ALLE, 'ohne Bit 0x02'),
    ('P1', SP, 'settings_file_set(&datei, "hand", 4, &hand, sizeof(hand)) == S_SUCCESS) {',
     'settings_file_set(&datei, "hanx", 4, &hand, sizeof(hand)) == S_SUCCESS) {', ZY, ALLE,
     'Zustand unter falschem Schluessel abgelegt'),
    ('P2', SP, '  prv_schlafmodus_laden();\n  prv_neues_fenster', '  prv_neues_fenster', ZY, ALLE,
     '"Laden" ohne den Schlafmodus'),
    ('I1', SS, '    .schlafmodus_seit = (uint32_t)boulder_schlafmodus_seit(),\n', '', SCH,
     ['schlafmodus_aus_der_app'], 'Lage ohne den Start'),
    ('I2', SS, '  if (boulder_schlafmodus_aktiv() != laeuft) {\n    return false;\n  }\n', '', SCH,
     ['schlafmodus_aus_der_app'], 'S8 schaltet auch gegen den Stand der App'),
    ('H1', BH, '  if (app->daten.lage.schlafmodus_seit) {\n    app->karte = BhKarteNight;\n  }\n', '', APP,
     APP_T, 'App oeffnet im Schlafmodus nicht auf Night'),
    ('H2', BH, '  if (app->karte == BhKarteNight && app->daten.lage.tracking && app->daten.lage.schlafmodus_seit) {',
     '  if (false) {', APP, APP_T, 'Select beendet ihn nicht'),
    ('H3', BH, '  if (app->karte == BhKarteNight && app->daten.lage.tracking) {\n    prv_schlafmodus(app);',
     '  if (app->daten.lage.tracking) {\n    prv_schlafmodus(app);', APP, APP_T,
     'Select lange auf jeder Karte'),
    ('H4', BH, '  if (!sys_boulder_schlafmodus_umschalten(laeuft)) {',
     '  if (!sys_boulder_schlafmodus_umschalten(!laeuft)) {', APP, APP_T, 'S8 mit dem falschen Stand'),
    ('H5', BH, '  window_long_click_subscribe(BUTTON_ID_SELECT, 0 /* Pebbles Vorgabe */, prv_lang, NULL);\n',
     '', APP, APP_T, 'Select lange nicht angemeldet'),
    ('C1', IN, '  if (d->lage.schlafmodus_seit) {\n    prv_schlafmodus(ctx, flaeche, d);\n    return;\n  }\n',
     '', KA, ['night_schlafmodus', 'night_schlafmodus_12h'], 'Night zeigt den Schlafmodus nicht'),
    ('T1', SW, '  return boulder_schlafmodus_aktiv();', '  return !boulder_schlafmodus_aktiv();', TS, None,
     'Schalter liest den Stand verkehrt'),
    ('T2', SW, '    .id = APP_ID_BOULDER_SCHLAF,', '    .id = APP_ID_HEALTH_APP,', TS, None,
     'S8 oeffnet die falsche App'),
    ('T3', SW, '    .set_exit_reason = true,', '    .set_exit_reason = false,', TS, None,
     'Schalter endet nicht zum Zifferblatt'),
]

def lauf(befehl, **kw):
    return subprocess.run(befehl, cwd=R, capture_output=True, text=True, **kw)


def runme(ziel):
    for p in (R / 'build-test').rglob('runme*'):
        if p.parent.name == ziel and p.is_file():
            return p
    raise SystemExit(f'runme fuer {ziel} fehlt')


def pruefen(ziel, tests):
    """Rueckgabe: Liste der roten Tests (leer = gruen), oder None, wenn der Bau scheitert."""
    bau = lauf(['ninja', '-C', 'build-test', ziel])
    if bau.returncode != 0:
        return None
    fehl = []
    for t in (tests or [None]):
        befehl = [str(runme(ziel))] + ([f'-t{t}'] if t else [])
        r = subprocess.run(befehl, cwd=runme(ziel).parent, capture_output=True, text=True,
                           timeout=900)
        if r.returncode != 0:
            fehl.append(t or 'alle')
    return fehl


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
        fehl = pruefen(ziel, tests)
        ergebnis = 'rot (Bau)' if fehl is None else (f'rot ({", ".join(fehl)})' if fehl else 'GRUEN')
    finally:
        pfad.write_text(original)
    rot += ergebnis.startswith('rot')
    zeile = f'{kennung:4s} {ergebnis:60s} {was} [{datei}, {ziel}]'
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
    r = subprocess.run([str(runme(z))], cwd=runme(z).parent, capture_output=True, text=True,
                       timeout=900)
    aus.write(f'{z} nach dem Rueckbau rc={r.returncode}\n')
    print(f'{z} nach dem Rueckbau rc={r.returncode}')
st = lauf(['git', 'status', '--porcelain'])
aus.write(f'Arbeitsbaum nach dem Rueckbau: {len(st.stdout.splitlines())} Aenderungen\n')
print(f'Arbeitsbaum nach dem Rueckbau: {len(st.stdout.splitlines())} Aenderungen')
