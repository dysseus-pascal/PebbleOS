/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// Boulder V2: Ablage und Laden (speicher.h).

#include "pbl/services/activity/boulder/speicher.h"

#include "pbl/services/activity/boulder/ausgabe.h"
#include "pbl/services/activity/boulder/diagnose.h"
#include "pbl/services/activity/boulder/eingang.h"
#include "pbl/services/activity/boulder/kontext.h"
#include "pbl/services/activity/boulder/messstufe.h"
#include "pbl/services/activity/boulder/nachlauf.h"
#include "pbl/services/activity/boulder/nacht.h"
#include "pbl/services/activity/boulder/takt.h"
#include "pbl/services/activity/boulder/umzug.h"
#ifdef CONFIG_BOULDER_HEALTH_APP
#include "pbl/services/activity/boulder/naechte.h"
#include "pbl/services/activity/boulder/ziele.h"
#endif
#include "pbl/services/settings/settings_file.h"
#include <pbl/drivers/rtc.h>
#include <pbl/logging/logging.h>

#include <string.h>

PBL_LOG_MODULE_DECLARE(service_activity, CONFIG_SERVICE_ACTIVITY_LOG_LEVEL);

#define BUCH_BLOCK_MIN 120
#define BUCH_BLOECKE (SCHLAF_MINUTEN_MAX / BUCH_BLOCK_MIN)
#define HRV_BLOCK 4
#define HRV_BLOECKE (SCHLAF_HRV_MAX / HRV_BLOCK)
#define PROT_BLOCK 4
#define PROT_BLOECKE ((AUSGABE_PROTOKOLL_TAKTE + PROT_BLOCK - 1) / PROT_BLOCK)
#define REST_MIN 15
#define REST_BLOECKE 4
#define REST_FEHLT 0xFFFF

typedef struct PBL_PACKED {
  uint32_t fenster;
  uint16_t bis;  //< fest bis zu dieser Minute (ab Minute 0 des Fensters)
  uint8_t buch[BUCH_BLOCK_MIN / 2];
} BuchBlock;

typedef struct PBL_PACKED {
  uint32_t fenster;
  uint8_t anzahl;
  NachtHrvDlsFenster fenster_[HRV_BLOCK];
} HrvBlock;

typedef struct PBL_PACKED {
  uint32_t fenster;
  uint8_t akku[PROT_BLOCK];
  uint8_t stufe[PROT_BLOCK];
} ProtBlock;

typedef struct PBL_PACKED {
  uint32_t fenster;
  uint16_t von;
  uint16_t vmc[REST_MIN];  //< REST_FEHLT = ungueltig
  uint8_t puls[REST_MIN];
} RestBlock;

// Was in der Datei steht (fuer "nur bei Aenderung"); gilt fuer s_fenster.
static time_t s_fenster;
// AP6: eine bestaetigte, laufende Fahrt (7.3, 7.4), unabhaengig vom Nachtfenster.
static BoulderFahrtAblage s_fahrt_datei;
// V3 (Schritt 2): die Ruhepulse der letzten sieben Naechte, auch unabhaengig vom Nachtfenster.
static BoulderRuheRing s_ruhe_datei;
static BoulderKopf s_kopf_datei;
static uint16_t s_buch_summe[BUCH_BLOECKE];
static uint16_t s_buch_bis[BUCH_BLOECKE];
static uint8_t s_hrv_anzahl[HRV_BLOECKE];
static uint16_t s_prot_summe[PROT_BLOECKE];
static uint16_t s_rest_bis;  //< die Minuten davor stehen im Ringrest

static bool s_alles;
static void prv_ablage_cb(void *data);
static BoulderAufgabe s_aufgabe_ablage = BOULDER_AUFGABE("Boulder Ablage", prv_ablage_cb);

static void prv_schluessel(char *s, char art, int k) {
  s[0] = art;
  s[1] = (char)('0' + k / 10);
  s[2] = (char)('0' + k % 10);
}

// Fletcher-16: billig, und ein vertauschtes Halbbyte faellt auf.
static uint16_t prv_summe(const uint8_t *d, size_t n, uint16_t start) {
  uint16_t a = start & 0xFF, b = start >> 8;
  for (size_t i = 0; i < n; i++) {
    a = (uint16_t)((a + d[i]) % 255);
    b = (uint16_t)((b + a) % 255);
  }
  return (uint16_t)((b << 8) | a);
}

static bool prv_setzen(SettingsFile *datei, char art, int k, const void *wert, size_t n) {
  char s[3];
  prv_schluessel(s, art, k);
  const status_t st = settings_file_set(datei, s, sizeof(s), wert, n);
  if (st != S_SUCCESS) {
    PBL_LOG_WRN("Boulder: Ablage %c%d nicht geschrieben (%d)", art, k, (int)st);
    return false;
  }
  return true;
}

static bool prv_holen(SettingsFile *datei, char art, int k, void *wert, size_t n) {
  char s[3];
  prv_schluessel(s, art, k);
  return settings_file_get_len(datei, s, sizeof(s)) == (int)n &&
         settings_file_get(datei, s, sizeof(s), wert, n) == S_SUCCESS;
}

static void prv_neues_fenster(time_t fenster) {
  s_fenster = fenster;
  memset(s_buch_summe, 0, sizeof(s_buch_summe));
  memset(s_buch_bis, 0, sizeof(s_buch_bis));
  memset(s_hrv_anzahl, 0, sizeof(s_hrv_anzahl));
  memset(s_prot_summe, 0, sizeof(s_prot_summe));
  s_rest_bis = 0;
}

// ---------------------------------------------------------------------------------------
// Ablage

static void prv_kopf(BoulderKopf *k) {
  memset(k, 0, sizeof(*k));
  boulder_nacht_kopf(k);
  const BoulderGesendet *g = boulder_ausgabe_gesendet();
  k->fassung = SPEICHER_FASSUNG;
  k->gesendet_beginn = g->beginn;
  k->gesendet_aufwachen = g->aufwachen;
  k->gesendet_schlaf = g->schlaf;
  k->geladen = boulder_ausgabe_protokoll_daten()->geladen;
}

static void prv_buch_ablegen(SettingsFile *datei, bool alles) {
  const uint16_t fest = boulder_nachlauf_fest_bis();
  const uint8_t *buch = boulder_nachlauf_buch();
  for (int k = 0; k < BUCH_BLOECKE && k * BUCH_BLOCK_MIN < fest; k++) {
    const int ende = (k + 1) * BUCH_BLOCK_MIN;
    const uint16_t bis = (uint16_t)((fest < ende) ? fest : ende);
    BuchBlock b = { .fenster = (uint32_t)s_fenster, .bis = bis };
    memcpy(b.buch, &buch[k * BUCH_BLOCK_MIN / 2], sizeof(b.buch));
    const uint16_t summe = prv_summe(b.buch, sizeof(b.buch), bis);
    if ((summe == s_buch_summe[k] && bis == s_buch_bis[k]) || (bis < ende && !alles)) {
      continue;
    }
    if (prv_setzen(datei, 'a', k, &b, sizeof(b))) {
      s_buch_summe[k] = summe;
      s_buch_bis[k] = bis;
    }
  }
}

static void prv_hrv_ablegen(SettingsFile *datei) {
  const BoulderEingang *e = boulder_eingang();
  if (boulder_nacht_stufe() == NachtErledigt) {
    return;  // die Felder gehoeren dem Tag
  }
  for (int k = 0; k < HRV_BLOECKE && k * HRV_BLOCK < e->hrv_anzahl; k++) {
    const int n = (e->hrv_anzahl - k * HRV_BLOCK < HRV_BLOCK) ? e->hrv_anzahl - k * HRV_BLOCK
                                                               : HRV_BLOCK;
    // Jede Ablage, sobald ein Fenster dazukam (hoechstens 25 B je Viertelstunde): ein Neustart
    // verliert sonst die HRV eines halben Blocks, und der Median der Nacht (0x4001) kippt.
    if (n == s_hrv_anzahl[k]) {
      continue;
    }
    HrvBlock b = { .fenster = (uint32_t)s_fenster, .anzahl = (uint8_t)n };
    for (int i = 0; i < n; i++) {
      const SchlafHrv *h = &e->hrv[k * HRV_BLOCK + i];
      b.fenster_[i] = (NachtHrvDlsFenster){ .minute = h->minute, .rmssd = h->rmssd,
                                           .puls = h->puls };
    }
    if (prv_setzen(datei, 'h', k, &b, sizeof(b))) {
      s_hrv_anzahl[k] = (uint8_t)n;
    }
  }
}

static void prv_protokoll_ablegen(SettingsFile *datei) {
  const BoulderProtokoll *p = boulder_ausgabe_protokoll_daten();
  if (p->fenster != s_fenster) {
    return;
  }
  for (int k = 0; k < PROT_BLOECKE; k++) {
    ProtBlock b = { .fenster = (uint32_t)s_fenster };
    memset(b.akku, AUSGABE_UNBEKANNT, sizeof(b.akku));
    memset(b.stufe, AUSGABE_UNBEKANNT, sizeof(b.stufe));
    bool bekannt = false;
    for (int i = 0; i < PROT_BLOCK && k * PROT_BLOCK + i < AUSGABE_PROTOKOLL_TAKTE; i++) {
      b.akku[i] = p->akku[k * PROT_BLOCK + i];
      b.stufe[i] = p->stufe[k * PROT_BLOCK + i];
      bekannt |= b.akku[i] != AUSGABE_UNBEKANNT;
    }
    // Jede Ablage, die einen Takt dazubrachte (12 B): der Akku beim Einschlafen steht erst bei
    // "fertig" fest, ein Neustart dazwischen soll ihn nicht verlieren.
    const uint16_t summe = prv_summe((const uint8_t *)&b, sizeof(b), 1);
    if (!bekannt || summe == s_prot_summe[k]) {
      continue;
    }
    if (prv_setzen(datei, 'p', k, &b, sizeof(b))) {
      s_prot_summe[k] = summe;
    }
  }
}

// Der Ringrest: jede volle Viertelstunde der Nacht, solange sie Schlaf hat - einmal.
static void prv_rest_ablegen(SettingsFile *datei) {
  const BoulderEingang *e = boulder_eingang();
  if (!boulder_nacht_rest_noetig()) {
    s_rest_bis = (e->letzte >= 0) ? (uint16_t)(e->letzte + 1) : 0;
    return;
  }
  const int bis = e->letzte + 1;
  int g = s_rest_bis / REST_MIN;
  // Nur die letzten REST_BLOECKE Viertelstunden zaehlen; aeltere stehen in Pebbles Datei.
  if (bis / REST_MIN - g > REST_BLOECKE) {
    g = bis / REST_MIN - REST_BLOECKE;
  }
  for (; (g + 1) * REST_MIN <= bis; g++) {
    RestBlock b = { .fenster = (uint32_t)s_fenster, .von = (uint16_t)(g * REST_MIN) };
    for (int i = 0; i < REST_MIN; i++) {
      const int m = g * REST_MIN + i;
      b.vmc[i] = e->gueltig[m] ? e->bewegung[m] : REST_FEHLT;
      b.puls[i] = e->puls[m];
    }
    if (!prv_setzen(datei, 'r', g % REST_BLOECKE, &b, sizeof(b))) {
      return;
    }
    s_rest_bis = (uint16_t)((g + 1) * REST_MIN);
  }
}

static void prv_ablage_cb(void *data) {
  BOULDER_AUFGABE_LAEUFT(&s_aufgabe_ablage);
  const bool alles = s_alles;
  s_alles = false;
  // AP5: die Messstufe in jeder Ablage nachlesen (ein ueberschriebener Wert kommt zurueck).
  boulder_messstufe_nachlesen();
#ifdef CONFIG_BOULDER_HEALTH_APP
  // Schritt 3: die Ziele ebenso, falls ein Ereignis der BlobDB verloren ging.
  boulder_ziele_nachlesen();
#endif
  boulder_ausgabe_bild_ablegen();
  // Vor dem Laden ist nichts Eigenes da - eine Ablage ueberschriebe den Stand von vor dem
  // Boot mit einem leeren.
  const time_t fenster = boulder_nacht_fenster();
  if (boulder_nacht_wartet() || !fenster) {
    return;
  }
  if (fenster != s_fenster) {
    prv_neues_fenster(fenster);
  }
  SettingsFile datei;
  if (settings_file_open(&datei, SPEICHER_DATEI, SPEICHER_DATEI_GROESSE) != S_SUCCESS) {
    PBL_LOG_WRN("Boulder: %s nicht geoeffnet", SPEICHER_DATEI);
    return;
  }
  // ERST DIE BLOECKE, DANN DER KOPF: ein Kopf, der "fertig" sagt, ohne das Buch dazu, liesse
  // nach einem Absturz dazwischen die Zyklen anders neu entstehen.
  prv_buch_ablegen(&datei, alles);
  prv_hrv_ablegen(&datei);
  prv_protokoll_ablegen(&datei);
  prv_rest_ablegen(&datei);
  boulder_diagnose_ablegen(&datei);
  BoulderFahrtAblage f;
  boulder_kontext_ablage(&f);
  if (memcmp(&f, &s_fahrt_datei, sizeof(f)) != 0 &&
      settings_file_set(&datei, "fahrt", 5, &f, sizeof(f)) == S_SUCCESS) {
    s_fahrt_datei = f;
  }
  BoulderRuheRing r;
  boulder_nacht_ruhe_ablage(&r);
  if (memcmp(&r, &s_ruhe_datei, sizeof(r)) != 0 &&
      settings_file_set(&datei, "ruhe", 4, &r, sizeof(r)) == S_SUCCESS) {
    s_ruhe_datei = r;
  }
#ifdef CONFIG_BOULDER_HEALTH_APP
  // Schritt 3: die Naechte der Health-App, nur nach einer Aenderung (eine endgueltige Nacht). Vor
  // dem Kopf: Ein Kopf, der die Nacht endgueltig nennt, steht nie ohne ihren Eintrag in der Datei.
  boulder_naechte_ablegen(&datei);
#endif
  BoulderKopf k;
  prv_kopf(&k);
  if (memcmp(&k, &s_kopf_datei, sizeof(k)) != 0 &&
      settings_file_set(&datei, "kopf", 4, &k, sizeof(k)) == S_SUCCESS) {
    s_kopf_datei = k;
  }
  settings_file_close(&datei);
}

void boulder_speicher_anstossen(bool alles) {
  s_alles |= alles;
  boulder_aufgabe_einreihen(&s_aufgabe_ablage);
}

// ---------------------------------------------------------------------------------------
// Laden

// Das Buch aus den Bloecken: Rueckgabe, bis wohin es lueckenlos fest ist.
static uint16_t prv_buch_laden(SettingsFile *datei) {
  boulder_nachlauf_neu(0, 0);
  uint8_t *buch = boulder_nachlauf_buch();
  uint16_t fest = 0;
  for (int k = 0; k < BUCH_BLOECKE; k++) {
    BuchBlock b;
    if (!prv_holen(datei, 'a', k, &b, sizeof(b)) || b.fenster != (uint32_t)s_fenster ||
        fest < k * BUCH_BLOCK_MIN || b.bis <= k * BUCH_BLOCK_MIN ||
        b.bis > (k + 1) * BUCH_BLOCK_MIN) {
      break;
    }
    memcpy(&buch[k * BUCH_BLOCK_MIN / 2], b.buch, sizeof(b.buch));
    fest = b.bis;
    s_buch_summe[k] = prv_summe(b.buch, sizeof(b.buch), b.bis);
    s_buch_bis[k] = b.bis;
  }
  return fest;
}

static void prv_hrv_laden(SettingsFile *datei) {
  for (int k = 0; k < HRV_BLOECKE; k++) {
    HrvBlock b;
    if (!prv_holen(datei, 'h', k, &b, sizeof(b)) || b.fenster != (uint32_t)s_fenster ||
        b.anzahl > HRV_BLOCK) {
      return;
    }
    for (int i = 0; i < b.anzahl; i++) {
      boulder_eingang_hrv(b.fenster_[i].minute, b.fenster_[i].rmssd, b.fenster_[i].puls);
    }
    s_hrv_anzahl[k] = b.anzahl;
    if (b.anzahl < HRV_BLOCK) {
      return;
    }
  }
}

static void prv_protokoll_laden(SettingsFile *datei) {
  BoulderProtokoll *p = boulder_ausgabe_protokoll_daten();
  for (int k = 0; k < PROT_BLOECKE; k++) {
    ProtBlock b;
    if (!prv_holen(datei, 'p', k, &b, sizeof(b)) || b.fenster != (uint32_t)s_fenster) {
      continue;
    }
    for (int i = 0; i < PROT_BLOCK && k * PROT_BLOCK + i < AUSGABE_PROTOKOLL_TAKTE; i++) {
      if (p->akku[k * PROT_BLOCK + i] == AUSGABE_UNBEKANNT) {
        p->akku[k * PROT_BLOCK + i] = b.akku[i];
        p->stufe[k * PROT_BLOCK + i] = b.stufe[i];
      }
    }
    s_prot_summe[k] = prv_summe((const uint8_t *)&b, sizeof(b), 1);
  }
}

// Der Ringrest auf den Eingang, aufsteigend; was D14 schon brachte, verwirft der Eingang
// (der erste Satz gilt).
static void prv_rest_laden(SettingsFile *datei) {
  RestBlock b[REST_BLOECKE];
  bool da[REST_BLOECKE];
  for (int k = 0; k < REST_BLOECKE; k++) {
    da[k] = prv_holen(datei, 'r', k, &b[k], sizeof(b[k])) && b[k].fenster == (uint32_t)s_fenster &&
            b[k].von + REST_MIN <= NACHT_MINUTEN;
  }
  for (;;) {
    int min = -1;
    for (int k = 0; k < REST_BLOECKE; k++) {
      if (da[k] && (min < 0 || b[k].von < b[min].von)) {
        min = k;
      }
    }
    if (min < 0) {
      return;
    }
    da[min] = false;
    for (int i = 0; i < REST_MIN; i++) {
      if (b[min].vmc[i] != REST_FEHLT) {
        boulder_eingang_minute(b[min].von + i, b[min].vmc[i], b[min].puls[i], true);
      }
    }
    s_rest_bis = (uint16_t)(b[min].von + REST_MIN);
  }
}

void boulder_speicher_laden(void) {
  prv_neues_fenster(boulder_nacht_fenster());
  SettingsFile datei;
  const bool offen =
      settings_file_open(&datei, SPEICHER_DATEI, SPEICHER_DATEI_GROESSE) == S_SUCCESS;
#ifdef CONFIG_BOULDER_HEALTH_APP
  // Schritt 3: DIE NAECHTE GELTEN UEBER JEDES FENSTER HINAUS, auch ohne eins - zuerst, damit der
  // Umzug (boulder_umzug_kopf) in die geladene Liste saet.
  boulder_naechte_laden(offen ? &datei : NULL);
#endif
  if (!s_fenster || !offen) {
    if (!offen) {
      PBL_LOG_WRN("Boulder: %s nicht geoeffnet, Nacht neu", SPEICHER_DATEI);
    } else {
      settings_file_close(&datei);
    }
    boulder_diagnose_laden(NULL, rtc_get_time());
    boulder_nacht_laden(NULL, 0);
    return;
  }
  boulder_diagnose_laden(&datei, rtc_get_time());
  BoulderFahrtAblage f;
  if (settings_file_get_len(&datei, "fahrt", 5) == (int)sizeof(f) &&
      settings_file_get(&datei, "fahrt", 5, &f, sizeof(f)) == S_SUCCESS) {
    s_fahrt_datei = f;
    boulder_kontext_laden(&f);
  }
  // DIE RUHEPULSE GELTEN UEBER DAS FENSTER HINAUS (V3): Laden vor der Nacht, die sie fuellt.
  BoulderRuheRing r;
  if (settings_file_get_len(&datei, "ruhe", 4) == (int)sizeof(r) &&
      settings_file_get(&datei, "ruhe", 4, &r, sizeof(r)) == S_SUCCESS) {
    boulder_nacht_ruhe_laden(&r);
    boulder_nacht_ruhe_ablage(&s_ruhe_datei);
  }
  BoulderKopf k;
  bool kopf = settings_file_get_len(&datei, "kopf", 4) == (int)sizeof(k) &&
              settings_file_get(&datei, "kopf", 4, &k, sizeof(k)) == S_SUCCESS &&
              k.fassung == SPEICHER_FASSUNG;
  const bool erster_start = !kopf;
  if (kopf) {
    s_kopf_datei = k;
  } else {
    // AP8: der erste Start nach b25 - der Kopf kommt aus b25s Zustand (umzug.c); in die Datei
    // schreibt ihn erst die naechste Ablage.
    PBL_LOG_INFO("Boulder: kein Kopf in %s", SPEICHER_DATEI);
    kopf = boulder_umzug_kopf(s_fenster, &k);
  }
  if (kopf) {
    // Was zuletzt geschickt wurde, gilt ueber das Fenster hinaus: dieselbe Nacht nie zweimal.
    *boulder_ausgabe_gesendet() = (BoulderGesendet){
      .beginn = k.gesendet_beginn, .aufwachen = k.gesendet_aufwachen,
      .schlaf = k.gesendet_schlaf,
    };
  }
  const bool gleich = kopf && k.fenster == (uint32_t)s_fenster;
  uint16_t fest = 0;
  if (gleich) {
    fest = prv_buch_laden(&datei);
    prv_hrv_laden(&datei);
    boulder_ausgabe_protokoll(s_fenster, -1, 0, 0, k.geladen, true);
    prv_protokoll_laden(&datei);
  }
  prv_rest_laden(&datei);
  settings_file_close(&datei);
  boulder_nacht_laden(gleich ? &k : NULL, fest);
  PBL_LOG_INFO("Boulder: geladen (%s, Buch fest bis %d)", gleich ? "Kopf" : "neu", fest);
  // Danach, im eigenen Callback: die letzte b25-Nacht in Pebbles Liste (Spezifikation 8).
  boulder_umzug_anstossen(erster_start);
}

void boulder_speicher_test_neustart(void) {
  prv_neues_fenster(0);
  memset(&s_kopf_datei, 0, sizeof(s_kopf_datei));
  memset(&s_fahrt_datei, 0, sizeof(s_fahrt_datei));
  memset(&s_ruhe_datei, 0, sizeof(s_ruhe_datei));
  s_alles = false;
  s_aufgabe_ablage.steht_an = false;
}
