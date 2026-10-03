# SPDX-FileCopyrightText: 2026 dysseus-pascal
# SPDX-License-Identifier: Apache-2.0

set(sources
  abschnitt.c
  art.c
  bahnen.c
  dienst.c
  kurve.c
  pausen.c
  plattform.c
  puls.c
  reps.c
  training.c
  wartend.c
)
pbl_library()
pbl_library_sources(${sources})
