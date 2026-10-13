# Wechsel b26 → V2 in QEMU (13.10.2026)

Punkt 2 aus Abschnitt 7 des Abnahmeberichts (`../../schritt3-abnahme.md`, Abschnitt 10).

## Ablauf

1. **b26 bauen:** Worktree von `main` `a7d8b16e1` (b26) unter `/home/user/pebbleos-b26`, eigenes venv,
   SDK 0.1.10 (b26 nennt 0.1.8; GCC 14 baut es ohne Warnung), `pbl configure --board qemu_emery`
   (`b26/bau.log`).
2. **Zustand in b26 erzeugen**, mit den Tasten über b26s Konsole (`click short`):
   - Health Tracking an, Distance Unit Kilometers, Precision (Messstufe) Detailed = Stufe 4;
   - Smart Alarm 07:21 an Werktagen, Basic Alarm 14:20 täglich;
   - Kieselsport: Running 1:22, gespeichert. Die Zusammenfassung wartet aufs Telefon, die Sitzung
     (Typ 6) steht in Pebbles Liste, Tag 84 hat einen Satz von 27 Byte (Fassung 7);
   - danach bis über Pebbles 15-Minuten-Sichern laufen lassen, Endstand in `b26/endstand.txt`.
3. **Wechsel:** QEMU beendet, `skripte/tauschen.py` schreibt V2s System-Ressourcen in Bank 0
   (0x620000, 2 MB). Alles andere bleibt, wie b26 es hinterlassen hat. Das Abbild vor dem Tausch
   liegt als `flash_b26_ende.bin.gz` bei.
4. **V2 starten:** normaler Bau für `qemu_emery` (`b6a4b3df`, SDK 0.1.11) auf diesem Flash; dann
   `sys reset` für einen zweiten Start.

`skripte/dienst.py` hält QEMU und die Konsole offen und liest Befehle aus einer Datei,
`sende.py` gibt sie hinein, `einmal.py` verbindet sich nach einem Neustart neu.

## Ergebnis

| Was | b26 | V2, erster Start | V2, zweiter Start |
|---|---|---|---|
| Health Tracking, Distanz, Precision | an, km, Detailed | gleich (Settings, Health-App: Karte Settings) | Messstufe 4 |
| Wecker | 07:21 smart Werktage, 14:20 täglich | beide geplant, App ohne Einführung | beide geplant |
| Pebbles Sitzungen | 1 (Running) | „Restored 1 activities“ | 1 |
| Datalogging 84/81/85 | 27/279/300 B | unverändert | unverändert |
| Kieselsport | Zusammenfassung wartet | erkannt, „Sending workout to phone …“ | – |
| Boulder | – | „kein Kopf“: Umzug einmal, b25/b26-Pref aufgeräumt | eigener Kopf, kein zweiter Umzug |
| Fehler | – | keine (nur Wetter ohne Daten, harter Neustart durch QEMU-Ende) | keine |

- **Zifferblatt:** V2 zeigt nach dem Wechsel Pebbles Standard-Zifferblatt. TimeStyle Bedrock gehört
  nicht mehr zur Firmware (Entscheid); das Original-TimeStyle kommt aus dem Store. Das gehört in
  die Release-Notiz.
- **Telefon in QEMU:** QEMU meldet eine offene Verbindung. Kieselsport versucht deshalb in b26 wie in V2
  alle ~12 s zu senden und wird abgelehnt (keine Boulder-App).
- **Nicht abgedeckt:** Nächte und Movement-Sitzungen (Typ 8) – QEMU liefert keine Sensordaten. Die
  decken die Host-Tests T8.1–T8.3 ab; b25 und b26 haben dieselben Formate (`umzug_pruefen.py`).

| Ordner/Datei | Inhalt |
|---|---|
| `b26/` | Antworten, Protokoll, Bilder jedes Schritts, Endstand (`pfs ls`, `dls list`), Bau |
| `v2/` | Antworten, Protokoll, Bootprotokolle beider Starts, Stände, Bilder (`h_alle.png`: Health-App) |
| `flash_b26_ende.bin.gz` | b26s Flash vor dem Wechsel (32 MB, gepackt) |
| `skripte/` | Dienst, Senden, Einmal-Verbindung, Tausch der Ressourcen |
