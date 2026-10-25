#!/usr/bin/env python3
"""b39: Mutationen der Stellen, die b39 neu oder anders hat: die Haken in Pebbles settings/health.c
und Boulders Zeilen dort (boulder_health_zeilen.c), dazu die nach v4.42 angepassten SpO2-Stellen
(Pebbles Satz spo2Preferences). Wie b38s mutationen.py: je Mutation die Quelle aendern, das Testziel
bauen, die genannten Tests laufen lassen - rot erwartet -, die Quelle zurueck. Am Ende alles wieder
gebaut und gruen.

Aufruf: mutationen.py <ausgabe.txt> [Kennung ...]
"""
import pathlib
import subprocess
import sys

R = pathlib.Path('/home/user/pebbleos-v2')
HEALTH = 'fw/apps/system/settings/health.c'
ZEILEN = 'fw/apps/system/settings/boulder_health_zeilen.c'
STUFE = 'fw/services/activity/boulder/messstufe.c'
EIN = 'test_boulder_einstellungen'
PEB = 'test_boulder_einstellungen_pebble'
MESS = 'test_boulder_messung'
PLAN = 'test_boulder_spo2_plan'

# (Kennung, Datei, alt, neu, Testziel, [Tests per -t] oder None fuer alle, Beschreibung)
M = [
    # Die Haken in Pebbles health.c
    ('H1', HEALTH, '      boulder_health_messstufe_zeile(&title, &subtitle);\n      break;\n', '', EIN,
     ['zeilen', 'precision'], 'Zeile HR Monitoring zeigt Pebbles Takt statt der Messstufe'),
    ('H2', HEALTH, '      boulder_health_spo2_schlaf_zeile(&title, &subtitle);\n      break;\n', '', EIN,
     ['zeilen'], 'Zeile SpO2 Monitoring statt SpO2 im Schlaf'),
    ('H3', HEALTH, '      boulder_health_messstufe_waehlen(context);\n      break;\n', '', EIN,
     ['precision'], 'Select oeffnet Pebbles Puls-Takt statt der Messstufe'),
    ('H4', HEALTH, '      boulder_health_spo2_schlaf_waehlen();\n      break;\n', '', EIN,
     ['schlaf_umschalten'], 'Select oeffnet Pebbles SpO2-Takt statt den Schalter'),
    ('H5', HEALTH, '#ifdef CONFIG_SERVICE_ACTIVITY_ALGORITHM_BOULDER  // Boulder: F3, die Zeile zeigt die Messstufe',
     '#ifndef CONFIG_SERVICE_ACTIVITY_ALGORITHM_BOULDER  // Boulder: F3, die Zeile zeigt die Messstufe',
     PEB, ['zeilen'], 'der Haken gilt auch ohne die Variante'),
    # Boulders Zeilen
    ('Z1', ZEILEN, 'boulder_messstufe_setzen((uint8_t)(selection + MESSSTUFE_MIN));',
     'boulder_messstufe_setzen((uint8_t)selection);', EIN, ['precision'], 'Wahl um eine Stufe verschoben'),
    ('Z2', ZEILEN, '*subtitle = boulder_messstufe_namen()[boulder_messstufe() - MESSSTUFE_MIN];',
     '*subtitle = boulder_messstufe_namen()[boulder_messstufe() % MESSSTUFE_MAX];', EIN, ['precision'],
     'Name der falschen Stufe'),
    ('Z3', ZEILEN, '                            boulder_messstufe() - MESSSTUFE_MIN, &callbacks, MESSSTUFE_MAX,',
     '                            0, &callbacks, MESSSTUFE_MAX,', EIN, ['precision'],
     'Auswahl beginnt nicht bei der geltenden Stufe'),
    ('Z4', ZEILEN, '                            boulder_messstufe() - MESSSTUFE_MIN, &callbacks, MESSSTUFE_MAX,',
     '                            boulder_messstufe() - MESSSTUFE_MIN, &callbacks, MESSSTUFE_MAX - 1,', EIN,
     ['precision'], 'Auswahl ohne Precise'),
    ('Z5', ZEILEN, 'boulder_spo2_schlaf_setzen(!boulder_spo2_schlaf());', 'boulder_spo2_schlaf_setzen(true);',
     EIN, ['schlaf_umschalten', 'zeile_vor_dem_ersten_lesen'], 'SpO2 im Schlaf laesst sich nicht ausschalten'),
    ('Z6', ZEILEN, '*subtitle = boulder_spo2_schlaf() ? i18n_noop("On") : i18n_noop("Off");',
     '*subtitle = boulder_spo2_schlaf() ? i18n_noop("Off") : i18n_noop("On");', EIN,
     ['zeilen', 'schlaf_umschalten', 'zeile_vor_dem_ersten_lesen'], 'Untertitel vertauscht'),
    ('Z7', ZEILEN, '*title = i18n_noop("SpO2 During Sleep");', '*title = i18n_noop("Blood O2 During Sleep");',
     EIN, ['zeilen'], 'Titel nicht wie Pebbles SpO2-Zeilen'),
    # Nach v4.42 angepasst
    ('S1', STUFE, ' || prv_schluessel(b, "hrmPreferences") ||\n      prv_schluessel(b, "spo2Preferences")) {',
     ' || prv_schluessel(b, "hrmPreferences")) {', MESS,
     ['regler_zieht_takte_zurueck_nicht_den_schalter'], 'Regler hoert den SpO2-Satz des Telefons nicht'),
    ('S2', 'fw/services/activity/activity.c',
     '  if (!boulder_spo2_plan_offen(activity_prefs_spo2_is_enabled()) ||',
     '  if (!boulder_spo2_plan_offen(false) ||', PLAN, None, 'Pebbles SpO2-Plan ohne den Schalter SpO2'),
    ('S3', 'fw/services/activity/boulder/schnittstelle.c',
     '  if (!activity_prefs_spo2_is_enabled() && !boulder_spo2_schlaf_geladen()) {',
     '  if (!boulder_spo2_schlaf_geladen()) {', 'test_boulder_schnittstelle', None,
     'Schnittstelle ohne den Schalter SpO2'),
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
