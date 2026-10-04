# SPDX-FileCopyrightText: 2026 dysseus-pascal
# SPDX-License-Identifier: Apache-2.0

# Boulder: Pruefungen der Werkzeuge unter tools/boulder.

# release.sh verweigert ein Release ohne gueltige Zeile "Mindest-App: X.Y.Z".
add_test(NAME test_boulder_release_mindest_app
  COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/test_release_mindest_app.sh ${PBL_BASE})
