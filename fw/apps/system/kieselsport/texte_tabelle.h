/* SPDX-FileCopyrightText: 2026 dysseus-pascal */
/* SPDX-License-Identifier: Apache-2.0 */
// Alle Texte der Uhr-App, eine Zeile je Text.
//
// ACHTUNG, ZWEI DINGE SIND ABSICHT:
//  1. KEIN #pragma once und keine Include-Waechter. Diese Datei wird MEHRFACH
//     eingebunden (X-Makro): einmal fuer die Aufzaehlung der Schluessel in
//     strings.h und einmal fuer die Tabelle in strings.c. Ein Waechter wuerde
//     die zweite Einbindung verschlucken und eine leere Tabelle erzeugen.
//  2. Endung .h, obwohl es kein gewoehnlicher Header ist: Build-Umgebungen,
//     die nur .c und .h in ihren Baum kopieren, faenden sie sonst nicht.
//
//   STR(schluessel, maxbytes, en, de, fr, it, es)
//
//   maxbytes  Groesse des Zielpuffers in BYTES, 0 wenn der Text in keinen
//             festen Puffer kopiert wird. Umlaute und Akzente zaehlen als
//             zwei Bytes, "·" und "…" als zwei bzw. drei. Bei Vorlagen mit %s
//             muss auch das Ergebnis mit dem laengsten Artnamen passen.
//   en        Englisch. Spalte 0 und zugleich der Rueckfall.
//
// KURZ HALTEN. Die schmalste Spalte ist die auf flint: 144 Punkte minus
// Leiste und Rand, gut hundert Punkte - in Gothic 14 um die siebzehn Zeichen,
// im fetten Titel keine dreizehn. Was laenger ist, endet in "…". Franzoesisch
// und Spanisch sind oft laenger als Deutsch; dort lieber ein Wort weniger.
//
// Nicht hier und absichtlich nicht uebersetzt: "Kieselsport" (der Name),
// die Einheiten km, m, kcal und "HRV ms" (Fachbegriff).
//
// Die Texte der Timeline-Pins und der Konfigseite stehen NICHT hier, sondern
// in src/pkjs: sie werden auf dem Telefon gebaut.

// ---- Sportarten (Menue, Kopfzeile) ---------------------------------------
// Die Reihenfolge der Arten steht in kern/art.c, nicht hier.
STR(STR_ART_LAUFEN,     0,  "Running",      "Laufen",         "Course",       "Corsa",         "Correr")
STR(STR_ART_BIKE,       0,  "Road/Gravel",  "Strasse/Gravel", "Route/Gravel", "Strada/Gravel", "Ruta/Gravel")
STR(STR_ART_WANDERN,    0,  "Hiking",       "Wandern",        "Randonnée",    "Escursione",    "Senderismo")
STR(STR_ART_KRAFT,      0,  "Strength",     "Kraft",          "Muscu",        "Pesi",          "Fuerza")
STR(STR_ART_MTB,        0,  "MTB",          "MTB",            "VTT",          "MTB",           "MTB")
STR(STR_ART_YOGA,       0,  "Yoga",         "Yoga",           "Yoga",         "Yoga",          "Yoga")
STR(STR_ART_SCHWIMMEN,  0,  "Swimming",     "Schwimmen",      "Natation",     "Nuoto",         "Natación")
// Obertitel im Menue unter Strasse/Gravel und MTB.
STR(STR_GRUPPE_BIKE,    0,  "Bike",         "Bike",           "Vélo",         "Bici",          "Bici")


// ---- Startschirm und Laden ------------------------------------------------
STR(STR_SELECT_STARTET, 0,  "Select starts", "Select startet", "Select démarre", "Select avvia", "Select inicia")
STR(STR_HOLE_STAND,     0,  "Loading …",    "Hole den Stand …", "Un instant …", "Un attimo …", "Cargando …")

// ---- Die Felder: Beschriftung in Grossbuchstaben --------------------------
// WIE IN DER WORKOUT-APP DER PEBBLE: ein Wort in Versalien ueber der grossen
// Zahl. Gross geschrieben steht es HIER, nicht per toupper - das kennt keine
// Umlaute und Akzente. Knapp: die Spalte ist auf flint gut hundert Punkte
// breit, in Gothic 18 fett rund elf Zeichen.
STR(STR_L_DAUER,        0,  "DURATION",     "DAUER",          "DURÉE",        "DURATA",        "DURACIÓN")
STR(STR_L_PULS,         0,  "HEART RATE",   "PULS",           "POULS",        "BATTITO",       "PULSO")
STR(STR_LETZTE_MINUTE,  0,  "Last minute",  "Letzte Minute",  "Dernière minute", "Ultimo minuto", "Último minuto")
STR(STR_L_DISTANZ,      0,  "DISTANCE",     "DISTANZ",        "DISTANCE",     "DISTANZA",      "DISTANCIA")
STR(STR_L_TEMPO,        0,  "PACE",         "TEMPO",          "ALLURE",       "PASSO",         "RITMO")
STR(STR_L_SCHRITTE,     0,  "STEPS",        "SCHRITTE",       "PAS",          "PASSI",         "PASOS")
STR(STR_L_KCAL,         0,  "CALORIES",     "KALORIEN",       "CALORIES",     "CALORIE",       "CALORÍAS")
STR(STR_L_WDH,          0,  "REPS",         "WDH.",           "RÉP.",         "RIP.",          "REP.")
STR(STR_L_PAUSE,        0,  "REST",         "PAUSE",          "REPOS",        "RECUPERO",      "DESCANSO")
STR(STR_L_SAETZE,       0,  "SETS",         "SÄTZE",          "SÉRIES",       "SERIE",         "SERIES")
STR(STR_L_GESAMT,       0,  "TOTAL REPS",   "WDH. GESAMT",    "RÉP. TOTAL",   "RIP. TOTALI",   "REP. TOTAL")
STR(STR_L_BAHNEN,       0,  "LAPS",         "BAHNEN",         "LONGUEURS",    "VASCHE",        "LARGOS")
STR(STR_L_UHR,          0,  "TIME",         "UHRZEIT",        "HEURE",        "ORA",           "HORA")
STR(STR_PAUSIERT,       0,  "PAUSED",       "PAUSE",          "EN PAUSE",     "IN PAUSA",      "EN PAUSA")

// ---- Die Zonen, als Beschriftung des Pulsfelds ----------------------------
// Statt "PULS" steht im Feld der Name der Zone, in der man gerade ist - wie
// FAT BURN / ENDURANCE / PERFORMANCE in der Workout-App, nur mit fuenf Zonen.
STR(STR_ZONE_1,         0,  "RECOVERY",     "ERHOLUNG",       "RÉCUP.",       "RECUPERO",      "RECUP.")
STR(STR_ZONE_2,         0,  "FAT BURN",     "GRUNDLAGE",      "FONCIER",      "FONDO",         "BASE")
STR(STR_ZONE_3,         0,  "ENDURANCE",    "AUSDAUER",       "ENDURANCE",    "RESISTENZA",    "RESISTENCIA")
STR(STR_ZONE_4,         0,  "THRESHOLD",    "SCHWELLE",       "SEUIL",        "SOGLIA",        "UMBRAL")
STR(STR_ZONE_5,         0,  "MAXIMUM",      "MAXIMUM",        "MAXIMUM",      "MASSIMO",       "MÁXIMO")

// ---- Nach dem Speichern: die Zeit in den Zonen ----------------------------
STR(STR_ZEIT_IN_ZONEN,  0,  "TIME IN ZONES", "ZEIT IN ZONEN", "TEMPS PAR ZONE", "TEMPO PER ZONA", "TIEMPO POR ZONA")

// ---- Laufender Schirm: die Fusszeile --------------------------------------
STR(STR_FUSS_SPEICHERE, 0,  "Saving …",     "Speichere …",    "Sauvegarde …", "Salvataggio …", "Guardando …")
STR(STR_FUSS_NOCHMAL,   0,  "Down again: discard", "Nochmal Unten: verwerfen", "Encore Bas : effacer", "Ancora Giù: scarta", "Otra vez Abajo: borrar")
STR(STR_FUSS_KEIN_TEL,  0,  "No phone: no distance", "Kein Telefon: keine Strecke", "Sans tél. : pas de distance", "Senza telefono, niente km", "Sin teléfono: sin distancia")
STR(STR_FUSS_AKKU,      0,  "Low battery: HR every 5 s", "Akku schwach: Puls alle 5 s", "Batterie faible : pouls/5 s", "Batt. bassa: polso ogni 5 s", "Batería baja: pulso cada 5 s")
STR(STR_FUSS_KOMPASS,   0,  "Compass not ready", "Kompass nicht bereit", "Boussole pas prête", "Bussola non pronta", "Brújula no lista")

// ---- Zusammenfassung nach dem Speichern -----------------------------------
STR(STR_GESPEICHERT,    0,  "Saved",        "Gespeichert",    "Enregistré",   "Salvato",       "Guardado")
STR(STR_TEL_AUFGEGEBEN, 0,  "Phone unreachable,\nsent on next launch", "Telefon nicht erreichbar,\ngeht beim nächsten Öffnen", "Téléphone absent,\nenvoi à la réouverture", "Telefono assente,\ninvio alla riapertura", "Sin teléfono,\nse envía al reabrir")
STR(STR_TEL_WARTET,     0,  "sending to phone …", "geht ans Telefon …", "envoi au téléphone …", "invio al telefono …", "enviando al teléfono …")
STR(STR_TEL_ANGEKOMMEN, 0,  "received by phone", "beim Telefon angekommen", "reçu par le téléphone", "ricevuto dal telefono", "recibido en el teléfono")

// ---- Das Training geht ans Telefon ----------------------------------------
STR(STR_TRAINING_SENDEN, 0, "Sending workout to phone …", "Training geht ans Telefon …", "Envoi de l'entraînement …", "Invio dell'allenamento …", "Enviando el entrenamiento …")
STR(STR_TRAINING_FERTIG, 0, "Workout sent", "Training übertragen", "Entraînement envoyé", "Allenamento inviato", "Entrenamiento enviado")

