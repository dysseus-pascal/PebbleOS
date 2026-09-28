/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

// Die eine Sperre des Dienstes. Der Kern ist nicht fuer mehrere Aufgaben
// geschrieben: der Takt laeuft auf KernelBG, der Kompass meldet sich auf
// KernelMain, und die App fragt aus ihrer eigenen Aufgabe. Alles, was den
// Kern anfasst, tut es unter dieser Sperre.
void ks_sperren(void);
void ks_entsperren(void);
