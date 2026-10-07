/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once
#include "plattform.h"

// Die Sportarten - und was bei jeder davon ueberhaupt eine Aussage ist.
//
// NICHT JEDE KENNZAHL PASST ZU JEDER ART. Schritte auf dem Bike sind eine
// Zufallszahl, und eine Distanz kann die Uhr ohne GPS nur aus Schritten
// schaetzen - beim Krafttraining ist beides sinnlos. Deshalb traegt jede Art
// mit, was sie kann, und der laufende Schirm zeigt nur das.
//
// Die Namen stehen in der App: der Dienst zeigt nichts an.

const ArtInfo *art_info(Sportart art);
