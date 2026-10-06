#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 dysseus-pascal
# SPDX-License-Identifier: Apache-2.0
"""Boulder V2 (AP-G): Referenzsaetze aus b26 erzeugen - nur Testdaten.

Aufruf: erzeugen.py <worktree von main (b26)>

Baut in dem Worktree tests/fw/services/activity/test_boulder_referenz.c aus dem
Nachbau der Uhr in test_boulder_nacht.c (bis "--- Die Nacht ---") und
referenz_teil.c und traegt den Test in die CMakeLists.txt dort ein. main bleibt
unveraendert: einen eigenen Worktree nehmen (git worktree add --detach). Danach:
  cmake -S tests -B build-ref && cmake --build build-ref --target test_boulder_referenz
  cd build-ref/fw/services/activity/test_boulder_referenz
  BOULDER_REFERENZ_AUS=<v2>/tests/fixtures/activity/boulder/referenz-b26 ./runme
Erzeugt am 06.10.2026 aus main a7d8b16e1.
"""
import pathlib
import sys

HIER = pathlib.Path(__file__).parent


def main(argv):
    if len(argv) != 2:
        print(__doc__, file=sys.stderr)
        return 2
    wt = pathlib.Path(argv[1])
    src = (wt / 'tests/fw/services/activity/test_boulder_nacht.c').read_text().split('\n')
    ende = src.index('// --- Die Nacht ' + '-' * 66)
    t = '\n'.join(src[:ende])

    def r(a, b):
        nonlocal t
        if t.count(a) != 1:
            raise SystemExit(f'Stelle nicht eindeutig: {a!r}')
        t = t.replace(a, b)

    i = t.index('int boulder_hrv_fenster(')
    r(t[i:t.index('#endif', i)], '''typedef struct { int minute; int rmssd; int puls; } RefHrv;
#define REF_HRV_MAX 400
static RefHrv s_ref_hrv[REF_HRV_MAX];
static int s_ref_hrv_anzahl;
int boulder_hrv_fenster(time_t von, time_t bis, SchlafHrv *aus, int max) {
  int n = 0;
  for (int k = 0; k < s_ref_hrv_anzahl && n < max; k++) {
    const time_t start = s_beginn + s_ref_hrv[k].minute * SECONDS_PER_MINUTE;
    if (start < von || start + 2 * 60 >= bis) {
      continue;
    }
    aus[n++] = (SchlafHrv){
      .minute = (start - von) / SECONDS_PER_MINUTE,
      .rmssd = s_ref_hrv[k].rmssd,
      .puls = s_ref_hrv[k].puls,
    };
  }
  return n;
}
''')
    r('  return (s_jetzt >= s_messstufe_5_ab) ? 5 : 3;', '  return s_ref_stufe;')
    r('static time_t s_messstufe_5_ab;', 'static time_t s_messstufe_5_ab;\nstatic uint8_t s_ref_stufe = 3;')
    r('static int s_saetze_anzahl;',
      'static int s_saetze_anzahl;\nstatic time_t s_satz_um[SAETZE];\nstatic time_t s_hrv_satz_um[SAETZE];')
    r('  memcpy(&s_saetze[s_saetze_anzahl++], data, sizeof(NachtDlsRecord));',
      '  s_satz_um[s_saetze_anzahl] = s_jetzt;\n'
      '  memcpy(&s_saetze[s_saetze_anzahl++], data, sizeof(NachtDlsRecord));')
    r('    memcpy(&s_hrv_saetze[s_hrv_saetze_anzahl++], data, sizeof(NachtHrvDlsRecord));',
      '    s_hrv_satz_um[s_hrv_saetze_anzahl] = s_jetzt;\n'
      '    memcpy(&s_hrv_saetze[s_hrv_saetze_anzahl++], data, sizeof(NachtHrvDlsRecord));')
    t += '\n' + (HIER / 'referenz_teil.c').read_text()
    (wt / 'tests/fw/services/activity/test_boulder_referenz.c').write_text(t)
    c = wt / 'tests/fw/services/activity/CMakeLists.txt'
    ct = c.read_text()
    if 'test_boulder_referenz' not in ct:
        c.write_text(ct + '''
pbl_clar_test(test_boulder_referenz
  SOURCES ${test_boulder_nacht_quellen}
  DEFINES CONFIG_SERVICE_KIESELSPORT ${BOULDER_AN}
  LIBS -Wl,--wrap=settings_file_set
)
''')
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
