#!/usr/bin/env python3
"""b37: Mutationen der SpO2-Schalter (Intervall, Schlaf, Sport). Je Mutation die Quelle aendern, das
Testziel bauen, die genannten Tests laufen lassen - rot erwartet -, die Quelle zurueck. Am Ende alles
wieder gebaut und gruen. Ein Ziel "skript:<datei>" ist ein Waechter aus tools/boulder (ctest ruft ihn
mit dem Repo); rot heisst dort Rueckgabe != 0.

Aufruf: mutationen.py <ausgabe.txt> [Kennung ...]
"""
import pathlib
import subprocess
import sys

R = pathlib.Path('/home/user/pebbleos-v2')
B = 'fw/services/activity/boulder/'
MESSUNG = 'test_boulder_messung'
PLAN = 'test_boulder_spo2_plan'
EINST = 'test_boulder_einstellungen'
SCHNITT = 'test_boulder_schnittstelle'

# (Kennung, Datei, alt, neu, Testziel, [Tests per -t] oder None fuer alle, Beschreibung)
M = [
    # Die Regel (spo2_plan.c)
    ('G1', B + 'spo2_plan.c', 'return intervall || (schlaf && nachtruhe);', 'return intervall || schlaf;',
     PLAN, ['regel_in_allen_faellen', 'schlaf_tags_nie'], 'Schlaf auch ohne Nachtruhe'),
    ('G2', B + 'spo2_plan.c', 'return intervall || (schlaf && nachtruhe);', 'return schlaf && nachtruhe;',
     PLAN, ['regel_in_allen_faellen', 'intervall_wie_pebble'], 'Intervall zaehlt nicht'),
    ('G3', B + 'spo2_plan.c', 'return intervall || (schlaf && nachtruhe);', 'return intervall || nachtruhe;',
     PLAN, ['regel_in_allen_faellen', 'nachtruhe_ohne_schalter_nie'], 'Nachtruhe ohne Schalter'),
    ('G4', B + 'spo2_plan.c', 'boulder_spo2_schlaf_geladen(), boulder_puls_nachtruhe());',
     'boulder_spo2_schlaf_geladen(), true);', MESSUNG, ['nachtruhe_fuer_den_spo2_plan'],
     'Regel ohne die Nachtruhe des Puls-Abos'),
    ('G5', B + 'spo2_plan.c', 'boulder_spo2_plan(intervall, boulder_spo2_schlaf_geladen(),',
     'boulder_spo2_plan(intervall, true,', MESSUNG, ['nachtruhe_fuer_den_spo2_plan'],
     'Regel ohne den geladenen Schalter'),
    # Pebbles Plan (activity.c)
    ('A1', 'fw/services/activity/activity.c',
     '  if (!boulder_spo2_plan_offen(activity_prefs_blood_oxygen_is_enabled()) ||',
     '  if (!activity_prefs_blood_oxygen_is_enabled() ||', PLAN,
     ['schlaf_in_nachtruhe', 'takt_der_stufe', 'ende_der_nachtruhe_beendet_die_messung'],
     'Plan fragt nur Pebbles Schalter'),
    ('A2', 'fw/services/activity/activity.c',
     '#ifdef CONFIG_SERVICE_ACTIVITY_ALGORITHM_BOULDER // Boulder: in Nachtruhe auch mit "Schlaf"\n'
     '  if (!boulder_spo2_plan_offen(activity_prefs_blood_oxygen_is_enabled()) ||\n#else\n'
     '  if (!activity_prefs_blood_oxygen_is_enabled() ||\n#endif\n',
     '  if (!boulder_spo2_plan_offen(activity_prefs_blood_oxygen_is_enabled()) || // Boulder: x\n',
     'skript:k1_waechter.py', None, 'Bedingung ohne den Schalter der Variante (K1)'),
    # Nachtruhe (puls.c)
    ('N1', B + 'puls.c', 'return s_tracking && s_nachtruhe;', 'return s_nachtruhe;', MESSUNG,
     ['nachtruhe_fuer_den_spo2_plan'], 'Nachtruhe auch ohne Tracking'),
    ('N2', B + 'puls.c', 'return s_tracking && s_nachtruhe;', 'return s_tracking;', MESSUNG,
     ['nachtruhe_fuer_den_spo2_plan'], 'jede Minute mit Tracking ist Nachtruhe'),
    # Der Schalter (messstufe.c)
    # Der Zweig fuer ungueltige Werte traegt die Regel: "return wert != 0" am Ende waere aequivalent.
    ('S1', B + 'messstufe.c', '%d ungueltig, gilt aus", wert);\n    return false;',
     '%d ungueltig, gilt aus", wert);\n    return true;', MESSUNG, ['schlaf_vom_telefon'],
     'ungueltiger Wert gilt als an'),
    ('S2', B + 'messstufe.c',
     '      PBL_LOG_WRN("Boulder: SpO2 im Schlaf nicht lesbar (%d), gilt aus", (int)st);\n    }\n'
     '    return false;',
     '      PBL_LOG_WRN("Boulder: SpO2 im Schlaf nicht lesbar (%d), gilt aus", (int)st);\n    }\n'
     '    return true;', MESSUNG, ['schlaf_vom_telefon'], 'ohne Schluessel an'),
    ('S3', B + 'messstufe.c', '      prv_schluessel(b, BOULDER_SPO2_SCHLAF_SCHLUESSEL) || ', '      ',
     MESSUNG, ['schlaf_neu_nach_seinem_ereignis', 'schlaf_vom_telefon'],
     'kein Lesen nach dem eigenen Ereignis'),
    ('S4', B + 'messstufe.c', 'const uint8_t schlaf = prv_schlaf_lesen() ? SCHLAF_AN : SCHLAF_AUS;',
     'const uint8_t schlaf = s_schlaf;', MESSUNG, ['schlaf_vom_telefon'],
     'Callback liest den Schalter nicht'),
    ('S5', B + 'messstufe.c',
     'return (schlaf == SCHLAF_UNGELESEN) ? prv_schlaf_lesen() : (schlaf == SCHLAF_AN);',
     'return schlaf == SCHLAF_AN;', EINST, ['zeile_vor_dem_ersten_lesen'],
     'Menue liest vor dem ersten Lesen nicht'),
    ('S6', B + 'messstufe.c', 'strlen(schluessel) + 1, &wert', 'strlen(schluessel), &wert', MESSUNG,
     ['schlaf_aus_dem_menue'], 'Schluessel ohne Nullbyte'),
    ('S7', B + 'messstufe.c', 'prv_schreiben(BOULDER_SPO2_SCHLAF_SCHLUESSEL, an ? 1 : 0);',
     'prv_schreiben(BOULDER_SPO2_SCHLAF_SCHLUESSEL, 1);', MESSUNG, ['schlaf_aus_dem_menue'],
     'Menue schreibt immer an'),
    ('S8', B + 'messstufe.c', '  prefs_private_lock();\n  SettingsFile datei;', '  SettingsFile datei;',
     MESSUNG, ['schlaf_aus_dem_menue'], 'Schreiben ohne Prefs-Sperre'),
    # Die Messstufe laesst "Blood Oxygen" (messstufe.c)
    ('B1', B + 'messstufe.c', '    activity_prefs_set_spo2_measurement_interval(spo2);\n  }\n  return true;',
     '    activity_prefs_set_spo2_measurement_interval(spo2);\n  }\n'
     '  if (!activity_prefs_blood_oxygen_is_enabled()) {\n'
     '    activity_prefs_set_blood_oxygen_enabled(true);\n  }\n  return true;', MESSUNG,
     ['stufe_vom_telefon_setzt_pebbles_werte', 'regler_zieht_takte_zurueck_nicht_den_schalter'],
     'Stufe schaltet Blood Oxygen wieder an'),
    ('B2', B + 'messstufe.c', '      prv_schluessel(b, "spo2Preferences")) {',
     '      prv_schluessel(b, "spo2Preferences") || prv_schluessel(b, "bloodOxygenPreferences")) {',
     MESSUNG, ['regler_zieht_takte_zurueck_nicht_den_schalter'],
     'bloodOxygenPreferences loest wieder Lesen aus'),
    ('B3', B + 'messstufe.c', '.hrv_takt_min = 0, .spo2_min = 60 },', '.hrv_takt_min = 0, .spo2_min = 0 },',
     MESSUNG, ['stufe_vom_telefon_setzt_pebbles_werte'], 'Stufe 1 ohne SpO2-Takt'),
    ('B4', B + 'messstufe.c', '.hrv_takt_min = 0, .spo2_min = 60 },', '.hrv_takt_min = 0, .spo2_min = 0 },',
     SCHNITT, ['messstufe_aus_der_app'], 'Stufe 1 ohne SpO2-Takt (Lage)'),
    # Pebbles HRM-Manager
    ('H1', 'fw/services/hrm/hrm_manager.c', '    allowed |= HRMFeature_SpO2;\n', '', MESSUNG,
     ['schlaf_erlaubt_spo2_im_manager'], 'Maske ohne den Schalter Schlaf'),
    ('H2', 'fw/services/hrm/hrm_manager.c',
     '      boulder_spo2_schlaf_geladen()) {\n    return true;\n  }',
     '      boulder_spo2_schlaf_geladen()) {\n  }', MESSUNG,
     ['schlaf_schaltet_den_sensor_ohne_puls'], 'Schlaf schaltet den Sensor nicht ein'),
    # Lage (schnittstelle.c)
    ('L1', B + 'schnittstelle.c',
     'if (!activity_prefs_blood_oxygen_is_enabled() && !boulder_spo2_schlaf_geladen()) {',
     'if (!activity_prefs_blood_oxygen_is_enabled()) {', SCHNITT, ['spo2_takt_auch_mit_schlaf'],
     'Lage sagt "off" trotz Schlaf'),
    # Menue (settings/health.c)
    ('E1', 'fw/apps/system/settings/health.c', 'boulder_spo2_schlaf_setzen(!boulder_spo2_schlaf());',
     'boulder_spo2_schlaf_setzen(true);', EINST, ['schlaf_umschalten'], 'Select schaltet nur an'),
    ('E2', 'fw/apps/system/settings/health.c',
     '  SettingsHealthBloodOxygenEnabled,\n#ifdef CONFIG_SERVICE_ACTIVITY_ALGORITHM_BOULDER'
     ' // Boulder: SpO2 im Schlaf; F3, kein SpO2-Takt\n  SettingsHealthBloodOxygenSleep,\n',
     '  SettingsHealthBloodOxygenSleep,\n  SettingsHealthBloodOxygenEnabled,\n'
     '#ifdef CONFIG_SERVICE_ACTIVITY_ALGORITHM_BOULDER // Boulder: SpO2 im Schlaf; F3, kein SpO2-Takt\n',
     EINST, ['zeilen'], 'Zeile vor statt nach Blood Oxygen'),
    ('E3', 'fw/apps/system/settings/health.c',
     '      subtitle = boulder_spo2_schlaf() ? i18n_noop("On") : i18n_noop("Off");',
     '      subtitle = activity_prefs_blood_oxygen_is_enabled() ? i18n_noop("On") : i18n_noop("Off");',
     EINST, ['schlaf_umschalten'], 'Zeile zeigt Pebbles Schalter'),
    # Whitelist
    ('W1', 'fw/services/blob_db/settings_blob_db.c',
     '  "boulderSpo2Schlaf", // Boulder: SpO2 im Schlaf (1 Byte, liest die Variante selbst)\n', '',
     'skript:vertraege.py', None, 'Schluessel fehlt in der Whitelist'),
    ('W2', 'fw/services/blob_db/settings_blob_db.c', '  "boulderSpo2Schlaf", //',
     '  // "boulderSpo2Schlaf", //', 'skript:vertraege.py', None,
     'Schluessel in der Whitelist auskommentiert'),
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
    if ziel.startswith('skript:'):
        skript = R / 'tools' / 'boulder' / ziel.split(':', 1)[1]
        r = lauf([sys.executable, str(skript), str(R)])
        return ['rc=%d' % r.returncode] if r.returncode else []
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
    zeile = f'{kennung:4s} {ergebnis:45s} {was} [{datei}, {ziel}]'
    print(zeile, flush=True)
    aus.write(zeile + '\n')
zeile = f'{rot}/{len(M)} rot'
print(zeile)
aus.write(zeile + '\n')
# Alles zurueck gebaut und gruen.
ziele = sorted({m[4] for m in M if not m[4].startswith('skript:')})
bau = lauf(['ninja', '-C', 'build-test'] + ziele)
aus.write(f'Rueckbau rc={bau.returncode}\n')
for z in ziele:
    r = subprocess.run([str(runme(z))], cwd=runme(z).parent, capture_output=True, text=True,
                       timeout=900)
    aus.write(f'{z} nach dem Rueckbau rc={r.returncode}\n')
    print(f'{z} nach dem Rueckbau rc={r.returncode}')
for s in sorted({m[4] for m in M if m[4].startswith('skript:')}):
    r = lauf([sys.executable, str(R / 'tools' / 'boulder' / s.split(':', 1)[1]), str(R)])
    aus.write(f'{s} nach dem Rueckbau rc={r.returncode}\n')
    print(f'{s} nach dem Rueckbau rc={r.returncode}')
st = lauf(['git', 'status', '--porcelain'])
aus.write(f'Arbeitsbaum nach dem Rueckbau: {len(st.stdout.splitlines())} Aenderungen\n')
print(f'Arbeitsbaum nach dem Rueckbau: {len(st.stdout.splitlines())} Aenderungen')
aus.close()
