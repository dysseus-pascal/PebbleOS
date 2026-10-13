// Rechnet eine exportierte Nacht (CSV aus Boulder) mit schlafanalyse.c nach (Host, ohne Uhr).
// Aufruf: nacht_rechnen <csv> [bis_minute]  -> Kopfzeile und Phasen je Minute, dazu der Vergleich
// mit der Phase der Uhr aus der CSV.
#include "pbl/services/activity/boulder/schlafanalyse.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint16_t s_vmc[SCHLAF_MINUTEN_MAX];
static uint8_t s_puls[SCHLAF_MINUTEN_MAX];
static bool s_gueltig[SCHLAF_MINUTEN_MAX];
static int s_phase_uhr[SCHLAF_MINUTEN_MAX];
static int s_schritte[SCHLAF_MINUTEN_MAX];
int s_schritte_im(int von, int bis) { int z = 0; for (int i = von; i < bis; i++) z += s_schritte[i]; return z; }
static SchlafHrv s_hrv[SCHLAF_HRV_MAX];
static SchlafArbeit s_arbeit;
static SchlafErgebnis s_erg;

int main(int argc, char **argv) {
  FILE *f = fopen(argv[1], "r");
  if (!f) {
    return 2;
  }
  const int bis = (argc > 2) ? atoi(argv[2]) : SCHLAF_MINUTEN_MAX;
  char z[256];
  int n = 0, nh = 0, teil = 0;
  int k_ein = -1, k_auf = -1, k_schlaf = -1, k_ruhe = -1, k_hrv = -1;
  while (fgets(z, sizeof(z), f)) {
    if (z[0] == '#') {
      char *p;
      if ((p = strstr(z, "einschlafen="))) k_ein = atoi(p + 12);
      if ((p = strstr(z, "aufwachen="))) k_auf = atoi(p + 10);
      if ((p = strstr(z, "schlaf="))) k_schlaf = atoi(p + 7);
      if ((p = strstr(z, "ruhepuls="))) k_ruhe = atoi(p + 9);
      if ((p = strstr(z, " hrv="))) k_hrv = atoi(p + 5);
      continue;
    }
    if (strncmp(z, "minute,", 7) == 0) { teil = 1; continue; }
    if (strncmp(z, "hrv_minute,", 11) == 0) { teil = 2; continue; }
    if (z[0] == '\n' || z[0] == '\r' || z[0] == 0) continue;
    if (teil == 1) {
      int m, s, v, p, l, ph; char uhr[8];
      if (sscanf(z, "%d,%7[^,],%d,%d,%d,%d,%d", &m, uhr, &s, &v, &p, &l, &ph) == 7 && m < bis) {
        s_vmc[m] = (uint16_t)v; s_puls[m] = (uint8_t)p; s_schritte[m] = s; s_gueltig[m] = true; s_phase_uhr[m] = ph;
        n = m + 1;
      }
    } else if (teil == 2) {
      int m, r, p; char uhr[8];
      if (sscanf(z, "%d,%7[^,],%d,%d", &m, uhr, &r, &p) == 4 && m < bis && nh < SCHLAF_HRV_MAX) {
        s_hrv[nh++] = (SchlafHrv){.minute = (uint16_t)m, .rmssd = (uint16_t)r, .puls = (uint8_t)p};
      }
    }
  }
  fclose(f);
  const SchlafEingabe e = {.bewegung = s_vmc, .puls = s_puls, .gueltig = s_gueltig,
                           .anzahl = (uint16_t)n, .schwelle = SCHLAF_SCHWELLE_VMC,
                           .hrv = s_hrv, .hrv_anzahl = (uint8_t)nh, .min_schlaf = 0};
  schlafanalyse_werte(&e, &s_arbeit, &s_erg);
  printf("rechnung gefunden=%d einschlafen=%u aufwachen=%u schlaf=%u ruhepuls=%u hrv=%u abschnitte=%u\n",
         s_erg.gefunden, s_erg.einschlafen, s_erg.aufwachen, s_erg.schlaf_minuten, s_erg.ruhepuls,
         s_erg.hrv, s_erg.abschnitt_anzahl);
  printf("uhr      einschlafen=%d aufwachen=%d schlaf=%d ruhepuls=%d hrv=%d\n", k_ein, k_auf, k_schlaf,
         k_ruhe, k_hrv);
  int ph[SCHLAF_MINUTEN_MAX];
  for (int i = 0; i < n; i++) ph[i] = -1;
  for (int a = 0; a < s_erg.abschnitt_anzahl; a++) {
    for (int i = s_erg.abschnitte[a].von; i < s_erg.abschnitte[a].bis && i < n; i++) {
      ph[i] = s_erg.abschnitte[a].phase;
    }
  }
  int anders = 0;
  for (int i = 0; i < n; i++) anders += (ph[i] != s_phase_uhr[i]);
  printf("phasen anders als die uhr: %d von %d\n", anders, n);
  printf("phasen ");
  for (int i = 0; i < n; i++) putchar(ph[i] < 0 ? '.' : '0' + ph[i]);
  printf("\nuhr    ");
  for (int i = 0; i < n; i++) putchar(s_phase_uhr[i] < 0 ? '.' : '0' + s_phase_uhr[i]);
  printf("\n");
  return 0;
}
