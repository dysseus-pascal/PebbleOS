# K2: Mehrwort-Speichern an der Syscall-Grenze

- `mehrwort.sh <pebbleos.elf>`: zählt je Kopierweg von S1–S7 die Speicherbefehle STM/STRD (ohne
  `sp`); Exit 1, wenn einer da ist. Grundlage für die Mutation G-K2 (`../mutationen.txt`).
- `vorher.txt`: obelix Release vor `11f0bb2a`, noch mit dem ersten Filter, der auch die Ladebefehle
  (LDM) zeigte. Speicherbefehle davon: S2 `__sys_boulder_lage` 2, S3 `boulder_spo2_werte` 1,
  S7 `boulder_woche_kopieren` 5.
- `nachher.txt`: obelix Release auf dem Endstand, überall 0.
