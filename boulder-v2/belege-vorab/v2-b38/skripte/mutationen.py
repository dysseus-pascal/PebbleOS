#!/usr/bin/env python3
"""b38: Mutationen der Untermenues (Bike, Swimming) und der Arten Indoor und Freiwasser, wie b37s
mutationen.py. Je Mutation die Quelle aendern, das Testziel bauen, die genannten Tests laufen
lassen - rot erwartet -, die Quelle zurueck. Am Ende alles wieder gebaut und gruen.

Aufruf: mutationen.py <ausgabe.txt> [Kennung ...]
"""
import pathlib
import subprocess
import sys

R = pathlib.Path('/home/user/pebbleos-v2')
K = 'fw/apps/system/kieselsport/'
D = 'fw/services/activity/boulder/sport/'
H = 'include/pbl/services/activity/boulder/sport/kieselsport.h'
ARTEN = 'test_kieselsport_arten'
DIENST = 'test_kieselsport'
FOLGEN = 'test_kieselsport_folgen'

INDOOR_IM_KOPF = ('  ArtAtem,\n  // Velo auf der Rolle oder dem Hometrainer: misst wie Strasse/Gravel, '
                  'aber das Telefon zeichnet\n  // keine Strecke auf.\n  ArtIndoor,\n')
FREI_IM_KOPF = ('  ArtIndoor,\n  // Schwimmen im offenen Wasser: ohne Bahnen und ohne Strecke.\n'
                '  ArtFreiwasser,\n')
OFFEN_IM_KOPF = '  ArtOffen,\n  // Dasselbe mit Strecke vom Telefon.\n  ArtOffenGps,\n'
HAUPT = ('  ArtLaufen, MENUE_BIKE, ArtWandern, ArtKraft, ArtYoga, MENUE_SCHWIMMEN, ArtAtem,\n'
         '  ArtOffen, ArtOffenGps,\n')
RAEDER = 'static const Sportart s_raeder[] = { ArtBike, ArtMTB, ArtIndoor };'
BAEDER = 'static const Sportart s_baeder[] = { ArtSchwimmen, ArtFreiwasser, ArtFreiwasserGps };'
IST = 'return eintrag >= MENUE_BIKE && eintrag < MENUE_BIKE + (int)ARRAY_LENGTH(s_gruppen);'
NAMEN = ('  STR_ART_INDOOR, STR_ART_FREIWASSER, STR_ART_FREIWASSER_GPS, STR_ART_OFFEN, '
         'STR_ART_OFFEN_GPS,\n};')
SYMBOLE = '  s_indoor, s_freiwasser, s_freiwasser, s_offen, s_offen,\n};'
INDOOR_ART = '{ false,    false,   false, false,  false },   // Indoor'
FREI_ART = '{ false,    false,   false, false,  false },   // Freiwasser\n'
FREI_GPS_ART = '{ false,    false,   false, false,  false },   // Freiwasser GPS'
OFFEN_ART = '{ true,     false,   false, false,  false },   // Offen\n'
OFFEN_GPS_ART = '{ true,     false,   false, false,  false },   // Offen GPS'
SZENE = ('if (art == ArtYoga || art == ArtSchwimmen || art == ArtAtem || art == ArtFreiwasser ||\n'
         '      art == ArtFreiwasserGps) {')

# (Kennung, Datei, alt, neu, Testziel, [Tests per -t] oder None fuer alle, Beschreibung)
M = [
    # Die Zahlen der Arten (kieselsport.h)
    ('Z1', H, INDOOR_IM_KOPF, '  ArtIndoor,\n  ArtAtem,\n', ARTEN, ['die_zahlen_bleiben'],
     'Indoor vor Atmen'),
    ('Z2', H, '  ArtMTB,\n', '  ArtMTB,\n  ArtNeu,\n', ARTEN, ['die_zahlen_bleiben'],
     'neue Art in der Mitte'),
    ('Z3', H, FREI_IM_KOPF, '  ArtFreiwasser,\n  ArtIndoor,\n', ARTEN, ['die_zahlen_bleiben'],
     'Freiwasser vor Indoor'),
    ('Z4', H, OFFEN_IM_KOPF, '  ArtOffenGps,\n  ArtOffen,\n', ARTEN, ['die_zahlen_bleiben'],
     'Custom GPS vor Custom'),
    # Die Menues und Gruppen (art_text.c)
    ('M1', K + 'art_text.c', RAEDER, 'static const Sportart s_raeder[] = { ArtBike, ArtMTB };', ARTEN,
     ['jede_art_genau_einmal_im_menue', 'die_gruppen', 'obertitel_nur_in_den_gruppen'],
     'Indoor fehlt bei den Raedern'),
    ('M2', K + 'art_text.c', BAEDER, BAEDER.replace('ArtFreiwasser, ', ''), ARTEN,
     ['jede_art_genau_einmal_im_menue', 'die_gruppen', 'obertitel_nur_in_den_gruppen'],
     'Open water fehlt beim Schwimmen'),
    ('M3', K + 'art_text.c', BAEDER, BAEDER.replace(', ArtFreiwasserGps', ''), ARTEN,
     ['jede_art_genau_einmal_im_menue', 'die_gruppen', 'obertitel_nur_in_den_gruppen'],
     'Open water GPS fehlt beim Schwimmen'),
    ('M4', K + 'art_text.c', HAUPT, HAUPT.replace('ArtAtem,', 'ArtAtem, ArtIndoor,'), ARTEN,
     ['jede_art_genau_einmal_im_menue'], 'Indoor auch im Hauptmenue'),
    ('M5', K + 'art_text.c', HAUPT, HAUPT.replace('MENUE_BIKE,', 'MENUE_BIKE, MENUE_BIKE,'), ARTEN,
     ['jede_art_genau_einmal_im_menue'], 'zwei Zeilen Bike'),
    ('M6', K + 'art_text.c', HAUPT, HAUPT.replace('MENUE_BIKE,', 'ArtBike,'), ARTEN,
     ['jede_art_genau_einmal_im_menue', 'die_gruppen'], 'Strasse/Gravel statt Bike im Hauptmenue'),
    ('M7', K + 'art_text.c', HAUPT, HAUPT.replace('MENUE_SCHWIMMEN,', 'ArtSchwimmen,'), ARTEN,
     ['jede_art_genau_einmal_im_menue', 'die_gruppen'], 'Pool statt Swimming im Hauptmenue'),
    ('M8', K + 'art_text.c', HAUPT, HAUPT.replace('  ArtOffen, ArtOffenGps,\n', '  ArtOffen,\n'), ARTEN,
     ['jede_art_genau_einmal_im_menue', 'die_gruppen'], 'Custom GPS fehlt im Hauptmenue'),
    ('M9', K + 'art_text.c', HAUPT, HAUPT.replace('ArtOffen, ArtOffenGps', 'ArtOffenGps, ArtOffen'), ARTEN,
     ['die_gruppen'], 'Custom GPS vor Custom im Hauptmenue'),
    ('M10', K + 'art_text.c', RAEDER, RAEDER.replace('ArtBike, ArtMTB', 'ArtMTB, ArtBike'), ARTEN,
     ['die_gruppen'], 'Raeder in anderer Reihenfolge'),
    ('M11', K + 'art_text.c', BAEDER, BAEDER.replace('ArtFreiwasser, ArtFreiwasserGps',
                                                     'ArtFreiwasserGps, ArtFreiwasser'), ARTEN,
     ['die_gruppen'], 'Open water GPS vor Open water'),
    ('M12', K + 'art_text.c', 's_haupt[zeile] : MENUE_BIKE;', 's_haupt[zeile] : ArtLaufen;', ARTEN,
     ['ausserhalb_der_listen'], 'Hauptmenue startet ausserhalb der Liste Laufen'),
    ('M13', K + 'art_text.c', 'g->arten[zeile] : g->arten[0];',
     'g->arten[zeile] : g->arten[g->anzahl - 1];', ARTEN, ['ausserhalb_der_listen'],
     'Untermenue faellt auf seine letzte Art zurueck'),
    ('M14', K + 'art_text.c', IST, IST.replace('< MENUE_BIKE +', '<= MENUE_BIKE +'), ARTEN,
     ['ausserhalb_der_listen'], 'eine Gruppe zu viel'),
    ('M15', K + 'art_text.c', IST, IST.replace('eintrag >= MENUE_BIKE &&', 'eintrag >= ArtOffenGps &&'),
     ARTEN, ['ausserhalb_der_listen', 'jede_art_genau_einmal_im_menue'], 'Custom GPS gilt als Gruppe'),
    ('M16', K + 'art_text.c', '? gruppe - MENUE_BIKE : 0]', '? gruppe - MENUE_BIKE : 1]', ARTEN,
     ['ausserhalb_der_listen'], 'unbekannte Gruppe ist das Schwimmen'),
    ('M17', K + 'art_text.c', '  }\n  return NULL;\n}', '  }\n  return S(STR_GRUPPE_BIKE);\n}', ARTEN,
     ['obertitel_nur_in_den_gruppen'], 'Obertitel auch ausserhalb der Gruppen'),
    ('M18', K + 'art_text.c', '  return S(prv_gruppe(gruppe)->name);', '  return S(STR_GRUPPE_BIKE);',
     ARTEN, ['die_gruppen'], 'jede Gruppe heisst Bike'),
    ('M19', K + 'art_text.c', '{ STR_GRUPPE_SCHWIMMEN, s_baeder,', '{ STR_GRUPPE_BIKE, s_baeder,', ARTEN,
     ['die_gruppen', 'obertitel_nur_in_den_gruppen'], 'Schwimmen unter dem Namen Bike'),
    ('M20', K + 'art_text.c', NAMEN, NAMEN.replace(' STR_ART_OFFEN_GPS,', ''), ARTEN,
     ['jede_art_hat_einen_namen', 'obertitel_nur_in_den_gruppen'], 'Custom GPS ohne Namen'),
    # Texte (texte_tabelle.h)
    ('T1', K + 'texte_tabelle.h', 'STR(STR_ART_INDOOR,     0,  "Indoor",',
     'STR(STR_ART_INDOOR,     0,  "",', ARTEN,
     ['jede_art_hat_einen_namen', 'obertitel_nur_in_den_gruppen'], 'englischer Name von Indoor leer'),
    ('T2', K + 'texte_tabelle.h', '"Indoor",         "Home trainer",', '"Indoor",         "VTT",',
     ARTEN, ['jede_art_hat_einen_namen'], 'Indoor franzoesisch wie MTB'),
    ('T3', K + 'texte_tabelle.h', 'STR(STR_ART_SCHWIMMEN,  0,  "Pool",',
     'STR(STR_ART_SCHWIMMEN,  0,  "Swimming",', ARTEN, ['obertitel_nur_in_den_gruppen'],
     'das Becken heisst wieder Swimming'),
    ('T4', K + 'texte_tabelle.h', '"Freiwasser",     "Eau libre",', '"Freiwasser",     "Piscine",', ARTEN,
     ['jede_art_hat_einen_namen'], 'Open water franzoesisch wie das Becken'),
    ('T5', K + 'texte_tabelle.h', '"Acque libere GPS", "Abiertas GPS")', '"Acque libere GPS", "Aguas abiertas")',
     ARTEN, ['jede_art_hat_einen_namen'], 'Open water GPS spanisch wie ohne GPS'),
    ('T6', K + 'texte_tabelle.h', '"Custom GPS",   "Frei GPS",', '"Custom GPS",   "Frei",', ARTEN,
     ['jede_art_hat_einen_namen'], 'Custom GPS deutsch wie Custom'),
    ('T7', K + 'texte_tabelle.h', 'STR(STR_ART_OFFEN,      0,  "Custom",', 'STR(STR_ART_OFFEN,      0,  "Workout",',
     ARTEN, ['obertitel_nur_in_den_gruppen'], 'Custom heisst anders'),
    # Farbe und Bild (thema.c, symbole.c)
    ('F1', K + 'thema.c', 'case ArtIndoor:    return GColorRajah;',
     'case ArtIndoor:    return GColorChromeYellow;', ARTEN, ['jede_art_hat_eine_farbe'],
     'Indoor in der Farbe von Strasse/Gravel'),
    ('F2', K + 'thema.c', 'case ArtFreiwasser: return GColorVividCerulean;',
     'case ArtFreiwasser: return GColorTiffanyBlue;', ARTEN, ['jede_art_hat_eine_farbe'],
     'Open water in der Farbe des Beckens'),
    ('F3', K + 'thema.c', 'case ArtFreiwasserGps: return GColorBlueMoon;',
     'case ArtFreiwasserGps: return GColorVividCerulean;', ARTEN, ['jede_art_hat_eine_farbe'],
     'Open water GPS in der Farbe von Open water'),
    ('F4', K + 'thema.c', '    case ArtOffen:     return GColorFolly;\n', '', ARTEN,
     ['jede_art_hat_eine_farbe'], 'Custom ohne eigene Farbe'),
    ('F5', K + 'thema.c', 'case ArtOffenGps:  return GColorFashionMagenta;',
     'case ArtOffenGps:  return GColorFolly;', ARTEN, ['jede_art_hat_eine_farbe'],
     'Custom GPS in der Farbe von Custom'),
    ('S1', K + 'symbole.c', 'LZ, 5, -14, 12, -14, -2, 0, -13, 14, -2, 14, 12,',
     'LZ, 5, -16, 12, -16, -2, 0, -13, 16, -2, 16, 12,', ARTEN, ['jede_art_hat_ein_symbol'],
     'Indoor: Waende ausserhalb des Rasters'),
    ('S2', K + 'symbole.c', 'LZ, 5, -13, -5, -8, -12, -4, -8, 0, -11, 4, -6,',
     'LZ, 5, -13, -5, -8, -16, -4, -8, 0, -11, 4, -6,', ARTEN, ['jede_art_hat_ein_symbol'],
     'Open water: Gipfel ueber dem Raster'),
    ('S3', K + 'symbole.c', SYMBOLE, SYMBOLE.replace(' s_offen, s_offen,', ' s_offen,'), ARTEN,
     ['jede_art_hat_ein_symbol'], 'Custom GPS ohne Bild'),
    # Wie die neuen Arten messen (art.c, plattform.c)
    ('A1', D + 'art.c', INDOOR_ART, INDOOR_ART.replace('{ false,', '{ true, ', 1), DIENST,
     ['t4_1_arten_ohne_schritte', 'indoor_misst_wie_strasse'], 'Indoor zaehlt Schritte'),
    ('A1f', D + 'art.c', INDOOR_ART, INDOOR_ART.replace('{ false,', '{ true, ', 1), FOLGEN, None,
     'Indoor zaehlt Schritte (Zufallsfolgen)'),
    ('A2', D + 'art.c', INDOOR_ART, '{ false,    true,    false, false,  false },   // Indoor',
     DIENST, ['indoor_misst_wie_strasse'], 'Indoor mit Strecke'),
    ('A3', D + 'art.c', INDOOR_ART, '{ false,    false,   false, false,  true  },   // Indoor',
     DIENST, ['indoor_misst_wie_strasse'], 'Indoor mit HRV'),
    ('A4', D + 'art.c', FREI_ART, '{ false,    false,   false, true,   false },   // Freiwasser\n',
     DIENST, ['freiwasser_ohne_bahnen'], 'Open water mit Bahnen'),
    ('A5', D + 'art.c', FREI_ART, FREI_ART.replace('{ false,', '{ true, ', 1), DIENST,
     ['t4_1_arten_ohne_schritte', 'freiwasser_ohne_bahnen'], 'Open water zaehlt Schritte'),
    ('A5f', D + 'art.c', FREI_ART, FREI_ART.replace('{ false,', '{ true, ', 1), FOLGEN, None,
     'Open water zaehlt Schritte (Zufallsfolgen)'),
    ('A6', D + 'art.c', FREI_GPS_ART, '{ false,    false,   false, true,   false },   // Freiwasser GPS',
     DIENST, ['freiwasser_gps_misst_wie_freiwasser'], 'Open water GPS mit Bahnen'),
    ('A7', D + 'art.c', OFFEN_ART, OFFEN_ART.replace('{ true, ', '{ false,', 1), DIENST,
     ['t4_1_arten_ohne_schritte', 'offen_wie_pebbles_workout'], 'Custom ohne Schritte'),
    ('A7f', D + 'art.c', OFFEN_ART, OFFEN_ART.replace('{ true, ', '{ false,', 1), FOLGEN, None,
     'Custom ohne Schritte (Zufallsfolgen)'),
    ('A8', D + 'art.c', OFFEN_GPS_ART, '{ true,     true,    false, false,  false },   // Offen GPS',
     DIENST, ['offen_wie_pebbles_workout'], 'Custom GPS mit Strecke der Uhr'),
    ('P1', D + 'plattform.c', SZENE, SZENE.replace(' || art == ArtFreiwasser ||', ' ||'), DIENST,
     ['freiwasser_ohne_bahnen'], 'Open water mit dem Modell fuer hohe Belastung'),
    ('P2', D + 'plattform.c', SZENE, SZENE.replace(' ||\n      art == ArtFreiwasserGps) {', ') {'), DIENST,
     ['freiwasser_gps_misst_wie_freiwasser'], 'Open water GPS mit dem Modell fuer hohe Belastung'),
    ('P3', D + 'plattform.c', SZENE, SZENE.replace('art == ArtFreiwasserGps) {',
                                                   'art == ArtFreiwasserGps || art == ArtOffen) {'),
     DIENST, ['offen_wie_pebbles_workout'], 'Custom mit dem allgemeinen Pulsmodell'),
    ('P4', D + 'plattform.c', SZENE, SZENE.replace('art == ArtFreiwasserGps) {',
                                                   'art == ArtFreiwasserGps || art == ArtIndoor) {'),
     DIENST, ['indoor_misst_wie_strasse'], 'Indoor mit dem allgemeinen Pulsmodell'),
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
