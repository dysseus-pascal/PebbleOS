// --- Referenz fuer Boulder V2 (AP-G) ---------------------------------------------
//
// Jede Fixture-Nacht laeuft Minute fuer Minute durch die b26-Nachtrechnung
// (nacht.c + schlafanalyse.c + activity_sessions.c, wie test_boulder_nacht).
// Ausgegeben werden die Eingabe, die Saetze 0x4001/0x4002 als Bytes, das
// NachtBild und die Phase je Minute aus dem letzten vollstaendigen Nachtsatz.

static int prv_spalte(char **namen, int anzahl, const char *name) {
  for (int k = 0; k < anzahl; k++) {
    if (!strcmp(namen[k], name)) {
      return k;
    }
  }
  return -1;
}

static int prv_trennen(char *zeile, char **feld, int max) {
  zeile[strcspn(zeile, "\r\n")] = 0;
  int k = 0;
  char *p = zeile;
  feld[k++] = p;
  while (*p && k < max) {
    if (*p == ',') {
      *p = 0;
      feld[k++] = p + 1;
    }
    p++;
  }
  return k;
}

static int prv_ab_20(const char *uhr) {
  const int m = atoi(uhr) * 60 + atoi(uhr + 3) - 20 * 60;
  return (m < 0) ? m + 24 * 60 : m;
}

static int s_ref_letzte;

// Die Nacht aus der Fixture: Minute aus der Uhrzeit (ab 20:00), VMC aus der
// Spalte vmc, Puls aus puls (0 = keiner). Fehlende Zeilen oder ein leeres vmc
// sind fehlende Minuten. Nach der letzten Zeile: wach und unterwegs, wie in
// test_boulder_nacht.
static void prv_ref_lade(const char *datei) {
  memset(s_minuten, 0xFF, sizeof(s_minuten));
  s_ref_hrv_anzahl = 0;
  char pfad[512];
  snprintf(pfad, sizeof(pfad), "%s/activity/boulder/%s", CLAR_FIXTURE_PATH, datei);
  FILE *f = fopen(pfad, "r");
  cl_assert(f);
  char zeile[512];
  char kopf[512] = { 0 };
  char *namen[16];
  int n_namen = 0;
  int sp_uhr = -1, sp_vmc = -1, sp_puls = -1;
  bool hrv_teil = false;
  s_ref_letzte = -1;
  while (fgets(zeile, sizeof(zeile), f)) {
    if (zeile[0] == '#') {
      continue;
    }
    if (zeile[0] == '\n' || zeile[0] == '\r' || zeile[0] == 0) {
      continue;
    }
    if (!strncmp(zeile, "minute,", 7)) {
      strncpy(kopf, zeile, sizeof(kopf) - 1);
      n_namen = prv_trennen(kopf, namen, 16);
      sp_uhr = prv_spalte(namen, n_namen, "uhrzeit");
      sp_vmc = prv_spalte(namen, n_namen, "vmc");
      sp_puls = prv_spalte(namen, n_namen, "puls");
      cl_assert(sp_uhr >= 0 && sp_vmc >= 0 && sp_puls >= 0);
      continue;
    }
    if (!strncmp(zeile, "hrv_minute,", 11)) {
      hrv_teil = true;
      continue;
    }
    char *feld[16] = { 0 };
    const int k = prv_trennen(zeile, feld, 16);
    if (hrv_teil) {
      cl_assert(k >= 4);
      cl_assert(s_ref_hrv_anzahl < REF_HRV_MAX);
      s_ref_hrv[s_ref_hrv_anzahl++] = (RefHrv){
        .minute = prv_ab_20(feld[1]), .rmssd = atoi(feld[2]), .puls = atoi(feld[3]),
      };
      continue;
    }
    cl_assert(k > sp_puls && k > sp_vmc);
    const int i = prv_ab_20(feld[sp_uhr]);
    cl_assert(i >= 0 && i < MINUTEN);
    if (!feld[sp_vmc][0]) {
      continue;
    }
    s_minuten[i] = (HealthMinuteData){
      .vmc = (uint16_t)atoi(feld[sp_vmc]),
      .heart_rate_bpm = feld[sp_puls][0] ? atoi(feld[sp_puls]) : 0,
    };
    if (i > s_ref_letzte) {
      s_ref_letzte = i;
    }
  }
  fclose(f);
  cl_assert(s_ref_letzte > 0);
  for (int i = s_ref_letzte + 1; i < MINUTEN; i++) {
    s_minuten[i] = (HealthMinuteData){ .vmc = 3000, .steps = 60, .heart_rate_bpm = 82 };
  }
}

static void prv_ref_reset(time_t beginn, uint8_t stufe) {
  s_beginn = beginn;
  s_jetzt = s_beginn;
  s_umstellung = 0x7FFFFFFF;
  s_messstufe_5_ab = 0x7FFFFFFF;
  s_ref_stufe = stufe;
  memset(&s_state, 0, sizeof(s_state));
  s_saetze_anzahl = 0;
  s_gesendet_um = 0;
  s_schreibzeiten = 0;
  s_geschickt = 0;
  s_geschickt_um = 0;
  s_hrv_anzahl = 0;
  s_hrv_saetze_anzahl = 0;
  s_hrv_gesendet_um = 0;
  s_tag84_anzahl = 0;
  s_training_von = 0;
  s_training_bis = 0;
  s_pause_ab = 0;
  s_anlage_scheitert = 0;
  s_nacht_scheitert = 0;
  s_anlagen_nacht = 0;
  s_heap_frei = 96 * 1024;
  s_heap_am_stueck = 64 * 1024;
  fake_malloc_set_largest_free_block(~0);
  s_bild_geschrieben = 0;
  s_viertelstuendlich = false;
  s_griffe = 0;
  s_griffe_max = 0;
  stub_new_timer_cleanup();
  s_num_new_timer_start_calls = 0;
  s_starts_gesehen = 0;
  s_zeitgeber_faellig = 0;
  fake_settings_file_reset();
  boulder_nacht_test_reset();
}

static void prv_hex(FILE *f, const void *daten, size_t n) {
  const uint8_t *b = daten;
  for (size_t k = 0; k < n; k++) {
    fprintf(f, "%02x", b[k]);
  }
}

static void prv_ref(const char *datei, time_t beginn, uint8_t stufe, const char *name) {
  prv_ref_reset(beginn, stufe);
  prv_ref_lade(datei);
  prv_durch_die_nacht();

  const char *aus = getenv("BOULDER_REFERENZ_AUS");
  cl_assert(aus);
  char pfad[512];
  snprintf(pfad, sizeof(pfad), "%s/%s.txt", aus, name);
  FILE *f = fopen(pfad, "w");
  cl_assert(f);
  fprintf(f, "# Boulder V2 Referenz (AP-G): die b26-Nachtrechnung auf %s.\n", datei);
  fprintf(f, "# Erzeugt aus main a7d8b16e1 (b26) mit dem Nachbau aus test_boulder_nacht.c:\n");
  fprintf(f, "# jede Minute activity_sessions_prv_minute_handler, dann boulder_nacht_minute,\n");
  fprintf(f, "# 20:00 bis 12:00 (Minuten 0..%d). Akku 80 %% um 20:00, -1 je Stunde; kein Training,\n", MINUTEN);
  fprintf(f, "# kein Ladekabel, Sommerzeit (UTC+2). Nach der letzten Zeile der Fixture wach und\n");
  fprintf(f, "# unterwegs (vmc 3000, Puls 82). NUR TESTDATEN, kein Richtigkeitsbeleg (Fitbit).\n");
  fprintf(f, "# eingabe <minute> <vmc> <puls>   nur gueltige Minuten; fehlende fehlen\n");
  fprintf(f, "# hrv <minute> <rmssd> <puls>     HRV-Fenster, wie boulder_hrv_fenster sie liefert\n");
  fprintf(f, "# satz4001 <minute gesendet> <328 Byte hex>, satz4002 <minute> <251 Byte hex>\n");
  fprintf(f, "# bild <512 Byte hex>             NachtBild am Ende (boulder_nacht_letzte)\n");
  fprintf(f, "# phasen <%d Zeichen>            je Minute ab 20:00: 0 wach 1 leicht 2 tief 3 REM\n", MINUTEN);
  fprintf(f, "#                                 4 Schlaf ohne Phase, . ausserhalb (letzter Satz)\n");
  fprintf(f, "quelle %s\nbeginn_utc %ld\nutc_to_local %d\nmessstufe %d\nminuten %d\n", datei,
          (long)beginn, OFFSET_LOKAL, stufe, MINUTEN);
  for (int i = 0; i < MINUTEN; i++) {
    if (s_minuten[i].vmc != 0xFFFF) {
      fprintf(f, "eingabe %d %d %d\n", i, s_minuten[i].vmc, s_minuten[i].heart_rate_bpm);
    }
  }
  for (int k = 0; k < s_ref_hrv_anzahl; k++) {
    fprintf(f, "hrv %d %d %d\n", s_ref_hrv[k].minute, s_ref_hrv[k].rmssd, s_ref_hrv[k].puls);
  }
  for (int k = 0; k < s_saetze_anzahl; k++) {
    fprintf(f, "satz4001 %ld ", (long)((s_satz_um[k] - beginn) / SECONDS_PER_MINUTE));
    prv_hex(f, &s_saetze[k], sizeof(NachtDlsRecord));
    fprintf(f, "\n");
  }
  for (int k = 0; k < s_hrv_saetze_anzahl; k++) {
    fprintf(f, "satz4002 %ld ", (long)((s_hrv_satz_um[k] - beginn) / SECONDS_PER_MINUTE));
    prv_hex(f, &s_hrv_saetze[k], sizeof(NachtHrvDlsRecord));
    fprintf(f, "\n");
  }
  NachtBild bild;
  memset(&bild, 0, sizeof(bild));
  cl_assert(boulder_nacht_letzte(&bild));
  fprintf(f, "bild ");
  prv_hex(f, &bild, sizeof(bild));
  fprintf(f, "\n");

  // Phase je Minute aus dem letzten vollstaendigen Nachtsatz.
  cl_assert(s_saetze_anzahl > 0);
  int letzter = -1;
  for (int k = s_saetze_anzahl - 1; k >= 0; k--) {
    if (s_saetze[k].teil == 0) {
      letzter = k;
      break;
    }
  }
  cl_assert(letzter >= 0);
  const NachtDlsRecord *s0 = &s_saetze[letzter];
  cl_assert(letzter + s0->teile <= s_saetze_anzahl);
  char phasen[MINUTEN + 1];
  memset(phasen, '.', MINUTEN);
  phasen[MINUTEN] = 0;
  for (int t = 0; t < s0->teile; t++) {
    const NachtDlsRecord *s = &s_saetze[letzter + t];
    cl_assert_equal_i(s->teil, t);
    cl_assert_equal_i(s->beginn_utc, s0->beginn_utc);
    for (int a = 0; a < s->anzahl; a++) {
      for (int m = s->abschnitte[a].von; m < s->abschnitte[a].bis; m++) {
        const int i = (int)((s->beginn_utc - beginn) / SECONDS_PER_MINUTE) + m;
        if (i >= 0 && i < MINUTEN) {
          phasen[i] = '0' + s->abschnitte[a].phase;
        }
      }
    }
  }
  fprintf(f, "phasen %s\n", phasen);
  fclose(f);
  printf("\nREFERENZ %s: saetze %d (hrv %d), letzter beginn %+ld min, einschlafen %d, aufwachen %d, "
         "schlaf %d, teile %d, ruhepuls %d, hrv %d, akku %d->%d\n", name, s_saetze_anzahl,
         s_hrv_saetze_anzahl, (long)((s0->beginn_utc - beginn) / 60), s0->einschlafen,
         s0->aufwachen, s0->schlaf_minuten, s0->teile, s0->ruhepuls, s0->hrv, s0->akku_beginn,
         s0->akku_ende);
}

void test_boulder_referenz__initialize(void) {
}

void test_boulder_referenz__nacht_2026_09_26(void) {
  prv_ref("nacht-2026-09-26.csv", 1790445600, 3, "nacht-2026-09-26");
}

void test_boulder_referenz__nacht_2026_09_29(void) {
  prv_ref("nacht-2026-09-29.csv", 1790445600 + 3 * SECONDS_PER_DAY, 4, "nacht-2026-09-29");
}

void test_boulder_referenz__nacht_2026_09_30(void) {
  prv_ref("nacht-2026-09-30.csv", 1790791200, 4, "nacht-2026-09-30");
}

void test_boulder_referenz__nacht_2026_10_03(void) {
  prv_ref("nacht-2026-10-03.csv", 1790964000, 4, "nacht-2026-10-03");
}

void test_boulder_referenz__nacht_2026_10_05(void) {
  prv_ref("nacht-2026-10-05.csv", 1791223200, 4, "nacht-2026-10-05");
}

void test_boulder_referenz__nacht_2026_10_05_morgen(void) {
  prv_ref("nacht-2026-10-05-morgen.csv", 1791223200, 4, "nacht-2026-10-05-morgen");
}
