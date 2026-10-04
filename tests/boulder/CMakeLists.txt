# SPDX-FileCopyrightText: 2026 dysseus-pascal
# SPDX-License-Identifier: Apache-2.0

# Boulder: Pruefungen der Werkzeuge unter tools/boulder.

# release.sh verweigert ein Release ohne gueltige Zeile "Mindest-App: X.Y.Z".
add_test(NAME test_boulder_release_mindest_app
  COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/test_release_mindest_app.sh ${PBL_BASE})

# Die mit Upstream geteilten Vertraege gegen die eingefrorene Liste - und ob die Wache
# anschlaegt, wo ein Upstream-Merge sie verletzen koennte.
add_test(NAME test_boulder_vertraege
  COMMAND ${PYTHON_EXECUTABLE} ${PBL_BASE}/tools/boulder/vertraege.py ${PBL_BASE})
add_test(NAME test_boulder_vertraege_wache
  COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_vertraege.py ${PBL_BASE})
