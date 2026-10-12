// Today-HRV wie inhalt.c:274-286, mit den Daten aus test_bh_karten (prv_voll), einmal mit und
// einmal ohne den Eintrag von heute (Nacht fertig, aber noch nicht endgueltig).
#include "rechnung.h"
#include <stdio.h>
#define HEUTE 9781
static void zeige(const char *name, BhNachtTag *tage, int n, uint16_t s1_hrv) {
  BhBandtag woche[BH_WOCHE_TAGE];
  bh_hrv_woche(tage, n, HEUTE, woche);
  const BhBandtag *t = &woche[BH_WOCHE_TAGE - 1];
  uint16_t hrv = t->wert ? t->wert : s1_hrv;
  const char *u = "(kein Urteil)";
  if (t->wert && bh_band_urteilt(t)) {
    uint16_t ab; BhVergleich v = bh_band_urteil(t, &ab);
    u = v == BhVergleichWieImmer ? "in range" : v == BhVergleichHoeher ? "above" : "below";
  }
  BhBandtag mit = *t; mit.wert = s1_hrv; uint16_t ab2;
  BhVergleich v2 = bh_band_urteil(&mit, &ab2);
  printf("%-28s Uhr: %u ms %s | Band %u-%u aus %u Werten | mit S1-HRV: %s\n", name, hrv, u,
         t->unten, t->oben, t->frueher,
         v2 == BhVergleichWieImmer ? "in range" : v2 == BhVergleichHoeher ? "above" : v2 == BhVergleichTiefer ? "below" : "-");
}
int main(void) {
  BhNachtTag tage[37]; int n = 0;
  for (int alter = 36; alter >= 0; alter--) {
    tage[n++] = (BhNachtTag){.tag = HEUTE - alter,
      .hrv = (uint16_t)(alter == 0 ? 56 : 46 + (alter * 7) % 13),
      .ruhepuls = (uint8_t)(alter == 0 ? 52 : 51 + (alter * 3) % 5)};
  }
  zeige("Liste mit heute", tage, n, 56);
  zeige("Liste ohne heute (fertig)", tage, n - 1, 56);
  return 0;
}
