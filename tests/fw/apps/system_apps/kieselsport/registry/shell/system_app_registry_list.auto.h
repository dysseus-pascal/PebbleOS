/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// Die Registry des Boards fuer test_kieselsport_startmenue: generate.py legt sie je Variante in
// den Bauordner (CMakeLists.txt), der Test nennt sie in KS_REGISTRY_LISTE. Dieser Ordner steht
// vor tests/overrides/default, dessen feste Test-Registry sonst gewaenne.
#include KS_REGISTRY_LISTE
