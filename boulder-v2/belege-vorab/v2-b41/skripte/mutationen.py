#!/usr/bin/env python3
"""b41: Mutationen des Zifferblatts Tagesband. Jede muss mindestens einen Test rot machen.

Je Mutation: eine Stelle genau einmal ersetzen, die drei Testziele bauen und laufen lassen, die
Datei zuruecksetzen. Rot = getoetet. Aufruf: mutationen.py <ausgabe.txt>
"""
import pathlib
import subprocess
import sys

R = pathlib.Path('/home/user/pebbleos-v2')
B = R / 'build-test'
BAND = 'fw/apps/system/boulder_tagesband/band.c'
BILD = 'fw/apps/system/boulder_tagesband/bild.c'
APP = 'fw/apps/system/boulder_tagesband/tagesband.c'
ZIELE = {
    'test_tagesband_band': B / 'fw/apps/system_apps/boulder_tagesband/test_tagesband_band/runme',
    'test_tagesband_bild_emery':
        B / 'fw/apps/system_apps/boulder_tagesband/test_tagesband_bild_emery/runme_emery',
    'test_tagesband_app': B / 'fw/apps/system_apps/boulder_tagesband/test_tagesband_app/runme',
}

M = [
    ('B1', BAND, '.von = jetzt - BT_VERGANGEN_S', '.von = jetzt - BT_SPANNE_S / 2',
     'Band halb Vergangenheit, halb Zukunft'),
    ('B2', BAND, 'const time_t a = MAX(von, b->von);', 'const time_t a = von;',
     'Strecke nicht am linken Rand geschnitten'),
    ('B3', BAND, 'const time_t e = MIN(bis, b->jetzt);', 'const time_t e = bis;',
     'Strecke reicht in die Zukunft'),
    ('B4', BAND, '    *rechts = *links + 1;\n', '    *rechts = *links;\n',
     'kurze Strecke ohne Mindestbreite'),
    ('B5', BAND, 'return GColorBlue; // tief', 'return GColorBlueMoon; // tief',
     'tief wie leicht'),
    ('B6', BAND, '    case ActivitySessionType_Nap:\n      return GColorBlueMoon;\n', '',
     'Nickerchen nicht im Band'),
    ('B7', BAND, 'nr > h->neueste || ', '',
     'Plaetze nach der neuesten Halbstunde (von gestern) gelten'),
    ('B8', BAND, '.neu = luecke}', '.neu = false}', 'Luecke im Puls ueberbrueckt'),
    ('B9', BAND, '.x = bt_x(b, MIN(mitte, b->jetzt))', '.x = bt_x(b, mitte)',
     'laufende Halbstunde hinter dem Jetzt'),
    ('B10', BAND, '*unten = (tief > 5) ? tief - 5 : 0;', '*unten = tief;', 'Skala ohne Rand'),
    ('B11', BAND, 'marke - bh_ortsversatz((time_t)(marke - versatz))', 'marke - versatz',
     'Marken ohne Zeitumstellung'),
    ('B12', BAND, 'if (utc > bis) {', 'if (utc >= bis) {', 'Marke am rechten Rand fehlt'),
    ('B13', BAND, "(stunde < 12) ? 'a' : 'p'", "(stunde < 12) ? 'p' : 'a'", 'a und p vertauscht'),
    ('B14', BAND, '#define TAGE_BIS_2000 730425', '#define TAGE_BIS_2000 730424',
     'Datum einen Tag daneben'),
    ('B15', BAND, '(stunde % 12 == 0) ? 12 : stunde % 12, min);', 'stunde % 12, min);',
     '12 h: Mitternacht und Mittag als 0'),
    ('B16', BAND, 'anzahl > 0 && tage[0].tag == d->heute', 'anzahl > 0',
     'Werte einer alten Nacht als heute'),
    ('G1', BILD, '  if (verdeckt) {\n    return;\n  }\n', '',
     'unter der Vorschau mit Zeile'),
    ('G2', BILD, 'schlaf ? GColorLightGray : GColorWhite', 'GColorWhite',
     'Zeit im Schlafmodus nicht gedimmt'),
    ('G3', BILD, 'ruhepuls ? GColorFolly : GColorLightGray', 'GColorFolly',
     'fehlender Ruhepuls in Rot'),
    ('G4', BILD, '  if (d->lage.schlafmodus_seit) {\n    prv_schlafmodus(ctx, b, d->lage.schlafmodus_seit);\n  }\n',
     '', 'Schlafmodus nicht im Band'),
    ('G5', BILD, 'i < n->anzahl && i < NACHT_BILD_ABSCHNITTE', 'i + 1 < n->anzahl',
     'letzte Phase der Nacht fehlt'),
    ('G6', BILD, '  if (schlaf) {\n    prv_schlafzeile(ctx, breite, d);\n  } else {\n    prv_werte(ctx, breite, d);\n  }\n',
     '  prv_werte(ctx, breite, d);\n', 'im Schlafmodus die Werte statt "Sleeping since"'),
    ('G7', BILD, '} else if (i + 1 == anzahl || p[i + 1].neu) {', '} else if (false) {',
     'einzelne Halbstunde ohne Punkt'),
    ('G8', BILD, 'ms ? hrv : fehlt', 'hrv', 'HRV 0 statt Strich'),
    ('A1', APP, '.is_unprivileged = true,', '.is_unprivileged = false,', 'mit Systemrechten'),
    ('A2', APP, '    app->daten.h24 = clock_is_24h_style();\n', '',
     'Wechsel 12/24 h erst mit der naechsten Minute'),
    ('A3', APP, '  if (app->daten.offen) {\n    app->nachladen =', '  if (false) {\n    app->nachladen =',
     'kein Nachladen nach dem Boot'),
    ('A4', APP, '  if (weiter) {', '  if (false) {', 'nur eine Nachfrage'),
    ('A5', APP, '  if (app->nachladen) {\n    app_timer_cancel(app->nachladen);\n  }\n', '',
     'offener Timer ueberlebt das Ende'),
    ('A6', APP, 'frei.size.h < flaeche.size.h', 'false', 'Vorschau nicht erkannt'),
    ('A7', APP, '    bh_daten_auffrischen(&app->daten);\n', '', 'Minutentakt liest nicht'),
]


def lauf():
    rot = []
    bau = subprocess.run(['cmake', '--build', str(B), '--target', *ZIELE], capture_output=True,
                         text=True)
    if bau.returncode:
        return ['Bau']
    for name, pfad in ZIELE.items():
        if subprocess.run([str(pfad)], cwd=pfad.parent, capture_output=True).returncode:
            rot.append(name)
    return rot


def main():
    aus = open(sys.argv[1], 'w')
    def schreib(t):
        print(t, flush=True)
        aus.write(t + '\n')
        aus.flush()
    vorher = lauf()
    schreib(f'Ausgangslage: {"rot " + ", ".join(vorher) if vorher else "gruen"}')
    if vorher:
        return 1
    ueberlebt = 0
    for mid, datei, alt, neu, was in M:
        p = R / datei
        text = p.read_text()
        if text.count(alt) != 1:
            schreib(f'{mid} STELLE {text.count(alt)}x: {was}')
            ueberlebt += 1
            continue
        try:
            p.write_text(text.replace(alt, neu))
            rot = lauf()
        finally:
            p.write_text(text)
        if rot:
            schreib(f'{mid} rot ({", ".join(rot)}): {was}')
        else:
            ueberlebt += 1
            schreib(f'{mid} UEBERLEBT: {was}')
    nachher = lauf()
    schreib(f'Danach: {"rot " + ", ".join(nachher) if nachher else "gruen"}')
    schreib(f'{len(M) - ueberlebt}/{len(M)} rot')
    return 1 if ueberlebt or nachher else 0


sys.exit(main())
