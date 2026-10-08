/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

// Die Registry des Boards fuer test_bh_startmenue: generate.py legt sie je Board in den Bauordner
// (CMakeLists.txt), der Test nennt sie in BH_REGISTRY_LISTE. Dieser Ordner steht vor
// tests/overrides/default, dessen feste Test-Registry sonst gewaenne.
#include BH_REGISTRY_LISTE
