"""AP9 Schritt 3: Mutationen je Abnahmekriterium (K1-K8) und Kernaussagen von Schritt 1 und 2.

Je Mutation ein Fehler in einer Datei; die genannte Pruefung muss rot werden. Die Datei kommt
danach aus dem Speicher zurueck (nicht aus git: uncommittete Arbeit bliebe sonst nicht stehen).
Aufruf: python3 mutationen.py [id ...]   (ohne id: alle)
"""
import os
import pathlib
import subprocess
import sys

R = pathlib.Path('/home/user/pebbleos-v2')
SDK = pathlib.Path('/root/pebbleos-sdk-0.1.10')
ENV = dict(os.environ)
ENV['PATH'] = f'{R}/.venv/bin:{SDK}/arm-none-eabi/bin:' + ENV['PATH']
APP = 'fw/apps/system/boulder_health/'
VAR = 'fw/services/activity/boulder/'


def ctest(regex):
    return ('ctest', regex)


def werkzeug(*args):
    return ('werkzeug', args)


def firmware(bau, *args):
    return ('firmware', bau, args)


def befehl(bau, *args):
    return ('befehl', bau, args)


MEHRWORT = '/home/user/pebbleos-notizen/boulder-v2/belege-schritt3/ap9/k2/mehrwort.sh'
OHNE_RECHTE = firmware('build-obelix', 'pruefe_ohne_rechte.py', str(R / 'build-obelix'), str(R))

# (id, Kriterium, Fehler, Datei, alt, neu, Pruefung)
M = [
    # K1 Pebble-Code nur P7
    ('K1a', 'K1', 'Pebble-Datei ausserhalb der Liste geaendert', 'fw/apps/system/health/health.c',
     '#include "health.h"\n', '#include "health.h"\n// Boulder: Mutation\n',
     werkzeug('k1_waechter.py', str(R))),
    ('K1b', 'K1', 'P7 bringt mehr als den Filter', 'fw/apps/system/CMakeLists.txt',
     '    list(FILTER source EXCLUDE REGEX "/apps/system/health/")\n',
     '    list(FILTER source EXCLUDE REGEX "/apps/system/health/")\n'
     '    list(APPEND source boulder_health/zusatz.c)\n',
     werkzeug('k1_waechter.py', str(R))),
    # K2 ohne Systemrechte
    ('K2a', 'K2', 'beschreibbares static in der App', APP + 'daten.c',
     'void bh_daten_laden(BhDaten *d) {\n',
     'void bh_daten_laden(BhDaten *d) {\n  static int s_mutation;\n  d->nachfragen += ++s_mutation;\n',
     OHNE_RECHTE),
    ('K2b', 'K2', 'Aufruf aus der roten Liste (Minutenverlauf, L5)', APP + 'daten.c',
     'void bh_daten_laden(BhDaten *d) {\n',
     'void bh_daten_laden(BhDaten *d) {\n  health_service_get_minute_history(NULL, 0, NULL, NULL);\n',
     OHNE_RECHTE),
    # K3 Grundsatz
    ('K3a', 'K3', 'Zonen ohne Quelle S6 gezeigt', APP + 'details.c',
     'const bool heute = d->herz_da && d->herz.zonen_tag == d->heute;',
     'const bool heute = d->herz.zonen_tag == d->heute;', ctest('^test_bh_grundsatz$')),
    ('K3b', 'K3', 'Text vom Telefon (Koffein in mg)', APP + 'texte_tabelle.h',
     'TEXT(BhTKcal, "kcal")', 'TEXT(BhTKcal, "mg")', ctest('^test_bh_grundsatz$')),
    # K4 gleiche Urteile wie Boulder
    ('K4a', 'K4', 'Schlafurteil: Grenze "kurz" 45 statt 60 min', APP + 'rechnung.c',
     '(int32_t)ziel_min - 60', '(int32_t)ziel_min - 45', ctest('^test_bh_rechnung$')),
    ('K4b', 'K4', 'Ruhepuls "as usual" mit 3 statt 2 bpm Spanne', APP + 'rechnung.c',
     '#define RUHEPULS_SPANNE   2', '#define RUHEPULS_SPANNE   3', ctest('^test_bh_rechnung$')),
    # K5 Einstiege
    ('K5a', 'K5', 'Quick Launch Oben gespiegelt statt Unten', APP + 'einstieg.c',
     '.gespiegelt = gerichtet && (knopf == BUTTON_ID_DOWN)',
     '.gespiegelt = gerichtet && (knopf == BUTTON_ID_UP)', ctest('^test_bh_einstiege?$')),
    ('K5b', 'K5', 'Pebbles Health-App nur noch mit dem Schalter (asterix ohne)',
     'fw/shell/normal/system_app_registry_list.json',
     '      "md_fn": "health_app_get_info",\n      "color_argb8": "GColorSunsetOrangeARGB8"\n',
     '      "md_fn": "health_app_get_info",\n      "color_argb8": "GColorSunsetOrangeARGB8",\n'
     '      "ifdefs": [\n        "CONFIG_BOULDER_HEALTH_APP"\n      ]\n',
     ctest('^test_bh_startmenue_asterix$')),
    # K6 Ladezeit (L3 auf dem Host; L1 als QEMU-Mutation getrennt)
    ('K6b', 'K6', 'Taste liest Daten (L3)', APP + 'boulder_health.c',
     '      BH_PROTOKOLL("karte %s", bh_karte_titel(app->karte));',
     '      bh_daten_auffrischen(&app->daten);\n'
     '      BH_PROTOKOLL("karte %s", bh_karte_titel(app->karte));', ctest('^test_bh_app$')),
    # K7 Ressourcen (Kernel-Heap als QEMU-Mutation getrennt)
    ('K7a', 'K7', 'Puls-Takt bleibt nach dem Ende', APP + 'boulder_health.c',
     '  prv_puls(app, false);\n  health_service_events_unsubscribe();',
     '  health_service_events_unsubscribe();', ctest('^test_bh_app$')),
    # K8 Ziele
    ('K8a', 'K8', 'Leeren der Datenbank Health laesst alte Ziele stehen', VAR + 'ziele.c',
     'const bool betrifft = (b->type == BlobDBEventTypeFlush) ||',
     'const bool betrifft = false ||', ctest('^test_boulder_schnittstelle$')),
    ('K8b', 'K8', 'Schlafurteil mit festem Ziel statt dem aus der Lage', APP + 'inhalt.c',
     'bh_schlafurteil(n->schlaf_minuten, d->lage.ziel_schlaf_min)',
     'bh_schlafurteil(n->schlaf_minuten, 420)', ctest('^test_bh_karten')),
    # Schritt 1
    # 30 statt 40 waere ein gleichwertiger Mutant: von 25 bis 60 rechnen alle Pruefnaechte gleich
    # (schlafanalyse.c). 20 ist der Wert von b25, der am 5.10. 29 Minuten zu frueh einschlafen liess.
    ('S1-3', 'Schritt 1 K3', 'Waelz-Regel beim Einschlafen wie b25: 20 statt 40 min', VAR + 'schlafanalyse.c',
     '#define EINSCHLAF_RUHE 40', '#define EINSCHLAF_RUHE 20', ctest('^test_boulder_(naechte|schlafkern)')),
    ('S1-4', 'Schritt 1 K4', 'Smart Alarm ohne Bewegung (auch im Tiefschlaf)', VAR + 'wecker.c',
     '  if (s_bewegt & WECKER_S1) {', '  if (true) {', ctest('^test_boulder_wecker')),
    ('S1-5', 'Schritt 1 K5', 'Velo nie erkannt', VAR + 'kontext.c',
     'if (m->schritte < KONTEXT_VELO_SCHRITTE_UNTER &&', 'if (m->schritte > KONTEXT_VELO_SCHRITTE_UNTER &&',
     ctest('^test_boulder_fahrt')),
    # Schritt 2
    ('S2-1', 'Schritt 2 V1', 'Velo-Training zaehlt Schritte', VAR + 'sport/plattform.c',
     'ks_schritte_streichen(!art_info(art)->schritte);', 'ks_schritte_streichen(false);',
     ctest('kieselsport')),
    ('S2-5', 'Schritt 2 K5', 'Pulskalorien: Ruheanteil 3,0 statt 3,5 ml/kg/min', VAR + 'kalorien.c',
     '#define VO2_RUHE_C 350', '#define VO2_RUHE_C 300', ctest('kalorien')),
]

# Gegenpruefung Schritt 3: je behobenem Befund die alte Stelle zurueck; der neue Test wird rot.
G = [
    ('G-A1', 'Gegenpruefung', 'Inhaltsebene nach ihrer ScrollLayer frei (A1)', APP + 'boulder_health.c',
     '  layer_destroy(app->detail_inhalt);\n  if (app->rolle) {\n    scroll_layer_destroy(app->rolle);\n  }\n',
     '  if (app->rolle) {\n    scroll_layer_destroy(app->rolle);\n  }\n  layer_destroy(app->detail_inhalt);\n',
     ctest('^test_bh_app$')),
    ('G-A2', 'Gegenpruefung', 'Puls-Abo ohne Ablauf im Kern (A2)', APP + 'boulder_health.c',
     '    sys_hrm_manager_set_update_interval(\n', '    (void)sys_hrm_manager_set_update_interval;\n    (\n',
     ctest('^test_bh_app$')),
    ('G-A4a', 'Gegenpruefung', 'Select liest neu (A4 a)', APP + 'boulder_health.c',
     '  BH_PROTOKOLL("detail %s", bh_detail_titel(detail));\n',
     '  bh_daten_auffrischen(&app->daten);\n  BH_PROTOKOLL("detail %s", bh_detail_titel(detail));\n',
     ctest('^test_bh_app$')),
    ('G-A4b', 'Gegenpruefung', 'Precision liest beim Setzen neu (A4 b)', APP + 'boulder_health.c',
     '  BH_PROTOKOLL("stufe %u", stufe);\n', '  bh_daten_auffrischen(&app->daten);\n  BH_PROTOKOLL("stufe %u", stufe);\n',
     ctest('^test_bh_app$')),
    ('G-A4c', 'Gegenpruefung', 'Puls live im 5-s-Takt (A4 c)', APP + 'boulder_health.c',
     'health_service_set_heart_rate_sample_period(an ? 1 : 0', 'health_service_set_heart_rate_sample_period(an ? 5 : 0',
     ctest('^test_bh_app$')),
    ('G-A5', 'Gegenpruefung', 'Detailhoehe nach neuen Daten nicht neu (A5)', APP + 'boulder_health.c',
     '    scroll_layer_set_content_size(app->rolle, GSize(breite, hoehe));\n', '    (void)hoehe;\n',
     ctest('^test_bh_app$')),
    ('G-A6', 'Gegenpruefung', 'Tracking aus oeffnet Details (A6)', APP + 'boulder_health.c',
     '(!app->daten.lage.tracking && detail != BhDetailPrecision)', '(false && detail != BhDetailPrecision)',
     ctest('^test_bh_app$')),
    ('G-A7', 'Gegenpruefung', 'Puls live auch im Detail (A7)', APP + 'boulder_health.c',
     'app->karte == BhKarteHeart && !app->detail_fenster &&', 'app->karte == BhKarteHeart &&',
     ctest('^test_bh_app$')),
    ('G-C1', 'Gegenpruefung', 'Urteil nie umbrochen (C1)', APP + 'zeichnen.c',
     '  if (bh_textbreite(text, BhSchriftG18Fett) <= breite) {', '  if (true) {',
     ctest('^test_bh_karten')),
    ('G-C1b', 'Gegenpruefung', 'Grenze ohne Layout zu eng (Very short night passt nicht)', APP + 'zeichnen.h',
     '#define BH_URTEIL_MAX  110', '#define BH_URTEIL_MAX  100', ctest('^test_bh_karten')),
    ('G-C2', 'Gegenpruefung', 'S1 nicht in der Naechteliste (C2)', APP + 'nachttage.c',
     '  if (!bh_nacht_da(d)) {\n    return anzahl;\n  }', '  if (true) {\n    return anzahl;\n  }',
     ctest('^test_bh_daten$')),
    ('G-C3', 'Gegenpruefung', 'Heart zeigt einen alten Wachtag (C3)', APP + 'inhalt.c',
     '(anzahl > 0 && tage[0].tag == heute)', '(anzahl > 0)', ctest('^test_bh_grundsatz$')),
    ('G-C4', 'Gegenpruefung', 'Folgenacht verdraengt die laengere Nacht (C4)', APP + 'nachttage.c',
     'd->nacht.schlaf_minuten > *minuten', 'd->nacht.schlaf_minuten > 0', ctest('^test_bh_daten$')),
    ('G-C5', 'Gegenpruefung', 'Uhrzeit mit dem Versatz des Fensterbeginns (C5)', APP + 'nachttage.c',
     '  return (int64_t)utc + bh_ortsversatz(utc);', '  return (int64_t)utc + n->utc_to_local;',
     ctest('^test_bh_daten$')),
    ('G-C6', 'Gegenpruefung', 'SpO2 nur aus 24 h (C6)', APP + 'daten.c',
     '    von = (nacht < von) ? nacht : von;', '    (void)nacht;', ctest('^test_bh_daten$')),
    ('G-C8', 'Gegenpruefung', 'Zonenminuten ohne Tausender (C8)', APP + 'details.c',
     '    bh_zahl(heute ? d->herz.zonen_min[i] : 0, BhSchriftGothic, zahl, sizeof(zahl));',
     '    snprintf(zahl, sizeof(zahl), "%u", heute ? d->herz.zonen_min[i] : 0);',
     ctest('^test_bh_grundsatz$')),
    ('G-A8', 'Gegenpruefung', 'Today urteilt ueber eine unfertige Nacht (A8)', APP + 'inhalt.c',
     '    if (fertig) {\n      prv_zeile(ctx, 32,', '    if (true) {\n      prv_zeile(ctx, 32,',
     ctest('^test_bh_grundsatz$')),
    ('G-K6', 'Gegenpruefung', 'Fertige Nacht im Minutentakt nicht neu gelesen (K6)', APP + 'daten.c',
     '      d->lage.nacht_stufe == NachtFertig || ', '      ', ctest('^test_bh_daten$')),
    # Gegenpruefung Kern und Werkzeug (K1-K5, A9)
    ('G-K1', 'Gegenpruefung', 'S3 prueft den Puffer mit max statt dem Begrenzten (K1)', VAR + 'schnittstelle.c',
     'syscall_assert_userspace_buffer(aus, n * sizeof(*aus));',
     'syscall_assert_userspace_buffer(aus, max * sizeof(*aus));', ctest('^test_boulder_schnittstelle$')),
    ('G-K2', 'Gegenpruefung', 'S2 kopiert per Strukturzuweisung (K2)', VAR + 'schnittstelle.c',
     '  memcpy(aus, &s_lage, sizeof(*aus));', '  *aus = s_lage;',
     befehl('build-obelixrel', MEHRWORT, str(R / 'build-obelixrel/pebbleos.elf'))),
    ('G-K3', 'Gegenpruefung', 'Laden nicht nachgeholt (K3)', VAR + 'dauerteil.c',
     '  if (s_laden_offen) {\n    boulder_aufgabe_einreihen(&s_aufgabe_laden);\n  }\n  s_letzte_minute_utc',
     '  if (false) {\n    boulder_aufgabe_einreihen(&s_aufgabe_laden);\n  }\n  s_letzte_minute_utc',
     ctest('^test_boulder_schnittstelle$')),
    ('G-K4', 'Gegenpruefung', 'Ziele-Filter nimmt auch laengere Schluessel (K4)', VAR + 'ziele.c',
     'b->key_len == n && memcmp', 'b->key_len >= n && memcmp', ctest('^test_boulder_schnittstelle$')),
    ('G-K5', 'Gegenpruefung', 'Uhr zurueck: kein Nachrechnen (K5)', VAR + 'herz.c',
     '  if (s_bereit && (s_herz.neueste >', '  if (false && (s_herz.neueste >', ctest('^test_boulder_herz$')),
    ('G-A9', 'Gegenpruefung', 'pruefe_ohne_rechte laesst jedes sys_* durch (A9)', 'tools/boulder/pruefe_ohne_rechte.py',
     "    if name.startswith('sys_') and name not in syscalls:\n        return False, 'kein Syscall'\n"
     "    if name in sdk or name in syscalls or",
     "    if name in sdk or name.startswith('sys_') or", werkzeug('test_pruefe_ohne_rechte.py')),
]
M_ALLE = M + G


def lauf(art):
    if art[0] == 'werkzeug':
        r = subprocess.run([sys.executable, str(R / 'tools/boulder' / art[1][0]), *art[1][1:]],
                           capture_output=True, text=True, env=ENV, check=False)
        return ('rot' if r.returncode else 'GRUEN'), r.stdout[-400:]
    if art[0] == 'befehl':
        b = subprocess.run(['cmake', '--build', str(R / art[1])], capture_output=True, text=True,
                           env=ENV, check=False)
        if b.returncode:
            return 'baut nicht', b.stdout[-800:]
        r = subprocess.run(list(art[2]), capture_output=True, text=True, env=ENV, check=False)
        return ('rot' if r.returncode else 'GRUEN'), r.stdout[-600:]
    if art[0] == 'firmware':
        b = subprocess.run(['cmake', '--build', str(R / art[1])], capture_output=True, text=True,
                           env=ENV, check=False)
        if b.returncode:
            return 'baut nicht', b.stdout[-800:]
        r = subprocess.run([sys.executable, str(R / 'tools/boulder' / art[2][0]), *art[2][1:]],
                           capture_output=True, text=True, env=ENV, check=False)
        return ('rot' if r.returncode else 'GRUEN'), '\n'.join(
            z for z in r.stdout.splitlines() if not z.startswith('ok '))
    b = subprocess.run(['cmake', '--build', str(R / 'build-test')], capture_output=True, text=True,
                       env=ENV, check=False)
    if b.returncode:
        return 'baut nicht', b.stdout[-800:]
    t = subprocess.run(['ctest', '-R', art[1]], cwd=R / 'build-test', capture_output=True,
                       text=True, env=ENV, check=False)
    gelaufen = [z for z in t.stdout.splitlines() if ' Test ' in z and '#' in z]
    if not gelaufen:
        return 'kein Test', t.stdout[-400:]
    return ('rot' if t.returncode else 'GRUEN'), '\n'.join(gelaufen)


def main(auswahl):
    ergebnis = []
    firmware_bauten = set()
    for mid, kriterium, name, datei, alt, neu, art in M_ALLE:
        if auswahl and mid not in auswahl:
            continue
        p = R / datei
        s = p.read_text()
        assert s.count(alt) == 1, f'{mid}: Stelle nicht eindeutig in {datei}'
        p.write_text(s.replace(alt, neu, 1))
        try:
            r, aus = lauf(art)
        finally:
            p.write_text(s)
        if art[0] in ('firmware', 'befehl'):
            firmware_bauten.add(art[1])
        ergebnis.append((mid, kriterium, name, datei, r))
        print(f'{r:10} {mid:5} {kriterium:13} {name} ({datei})', flush=True)
        for z in aus.strip().splitlines()[-6:]:
            print(f'           | {z}', flush=True)
    # Die Bauordner wieder auf den echten Stand bringen.
    for bau in sorted(firmware_bauten):
        subprocess.run(['cmake', '--build', str(R / bau)], capture_output=True, env=ENV, check=True)
    subprocess.run(['cmake', '--build', str(R / 'build-test')], capture_output=True, env=ENV,
                   check=True)
    rot = sum(r == 'rot' for *_, r in ergebnis)
    print(f'{rot}/{len(ergebnis)} rot' + ('' if rot == len(ergebnis) else ' - NICHT ALLE ROT'))
    return 0 if rot == len(ergebnis) else 1


if __name__ == '__main__':
    sys.exit(main(set(sys.argv[1:])))
