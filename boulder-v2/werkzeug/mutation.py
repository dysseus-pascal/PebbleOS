"""Mutationen: Je Kernaussage einen Fehler einbauen, betroffene Tests muessen rot werden."""
import subprocess, sys, pathlib, os
R = pathlib.Path('/home/user/pebbleos-v2')
APP = R / 'fw/apps/system/boulder_health'
ENV = dict(os.environ)
ENV['PATH'] = f"{R}/.venv/bin:/root/pebbleos-sdk-0.1.10/arm-none-eabi/bin:" + ENV['PATH']
M = [
  ('Urteilsfarbe vertauscht', 'inhalt.c', 'return gut ? BH_GUT : BH_AUFFAELLIG;', 'return gut ? BH_AUFFAELLIG : BH_GUT;', 'test_bh_karten'),
  ('Schlafziel nicht aus der Lage', 'inhalt.c', 'bh_schlafurteil(d->nacht.schlaf_minuten, d->lage.ziel_schlaf_min)', 'bh_schlafurteil(d->nacht.schlaf_minuten, 420)', 'test_bh_karten'),
  ('Tracking aus uebersehen', 'inhalt.c', 'if (!d->lage.tracking && karte != BhKarteSettings) {', 'if (false) {', 'test_bh_karten'),
  ('SpO2 off ohne Lage', 'inhalt.c', '(d->lage_da && d->lage.spo2_takt_min == 0)', '(d->lage.spo2_takt_min == 0)', 'test_bh_karten'),
  ('HRV ohne Nacht von heute', 'inhalt.c', 'const uint16_t hrv = !heute ? 0', 'const uint16_t hrv = false ? 0', 'test_bh_grundsatz'),
  ('Auffrischen liest Nacht jede Minute', 'daten.c', "if (!war_da || d->lage.nacht_stufe != stufe", "if (true || d->lage.nacht_stufe != stufe", 'test_bh_daten'),
  ('Nachladen ohne Grenze', 'daten.c', 'if (!d->offen || d->nachfragen >= BH_NACHFRAGEN) {', 'if (!d->offen) {', 'test_bh_daten'),
  ('Ring vorwaerts gelesen', 'nachttage.c', '(d->naechte.neu + NAECHTE_MAX - i) % NAECHTE_MAX', '(d->naechte.neu + i) % NAECHTE_MAX', 'test_bh_daten'),
  ('Taste liest Daten', 'boulder_health.c', '      BH_PROTOKOLL("karte %s", bh_karte_titel(app->karte));', '      bh_daten_auffrischen(&app->daten);\n      BH_PROTOKOLL("karte %s", bh_karte_titel(app->karte));', 'test_bh_app'),
  ('Puls-Takt bleibt nach dem Ende', 'boulder_health.c', '  prv_puls(app, false);\n  health_service_events_unsubscribe();', '  health_service_events_unsubscribe();', 'test_bh_app'),
  ('Puls live ohne Zeitgrenze', 'boulder_health.c', '    app->puls_ende = app_timer_register(PULS_LIVE_MS, prv_puls_ende, NULL);', '    (void)prv_puls_ende;', 'test_bh_app'),
  ('Today oeffnet ein Detail', 'karten.c', '    default: return BhDetailKeins;  // Today (F12)', '    default: return BhDetailNight;', 'test_bh_app'),
  ('Precision setzt falsche Stufe', 'boulder_health.c', 'const uint8_t stufe = app->auswahl + 1;', 'const uint8_t stufe = app->auswahl;', 'test_bh_app'),
  ('Detail ohne Freigabe der ScrollLayer', 'boulder_health.c', '    scroll_layer_destroy(app->rolle);', '    (void)0;', 'test_bh_app'),
  ('Kartenfarbe ohne Kontrast', 'karten.c', 'case BhKarteHeart: return GColorFolly;', 'case BhKarteHeart: return GColorMelon;', 'test_bh_karten'),
]
def lauf(ziel):
    b = subprocess.run(['cmake', '--build', str(R / 'build-test')], capture_output=True, text=True, env=ENV)
    if b.returncode:
        return 'baut nicht'
    t = subprocess.run(['ctest', '-R', ziel], cwd=R / 'build-test', capture_output=True, text=True, env=ENV)
    return 'rot' if t.returncode else 'GRUEN'
ergebnis = []
for name, datei, alt, neu, ziel in M:
    p = APP / datei
    s = p.read_text()
    assert alt in s, name
    p.write_text(s.replace(alt, neu, 1))
    try:
        r = lauf(ziel)
    finally:
        p.write_text(s)
    ergebnis.append((name, ziel, r))
    print(f'{r:10} {name} ({ziel})', flush=True)
print('alle rot' if all(r != 'GRUEN' for _, _, r in ergebnis) else 'NICHT ALLE ROT')
