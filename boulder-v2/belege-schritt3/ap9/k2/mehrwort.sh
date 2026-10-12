#!/bin/bash
# K2-Beleg: Mehrwort-Speichern (STM/STRD fordern Wortausrichtung; LDM/LDRD lesen nur Kern-Daten) in den Kopierwegen der
# Syscalls S1-S7. push/pop (auf sp) zeigt objdump als push/pop, sie zaehlen nicht. Exit 1, wenn eins da ist.
. ~/pebbleos-sdk-0.1.10/env.sh
ELF=$1
summe=0
for f in __sys_boulder_nacht boulder_nacht_bild_kopieren __sys_boulder_lage __sys_boulder_spo2 boulder_spo2_werte __sys_boulder_naechte boulder_naechte_kopieren __sys_boulder_herz boulder_herz_kopieren __sys_boulder_woche boulder_woche_kopieren; do
  n=$(arm-none-eabi-objdump -d --no-show-raw-insn "$ELF" --disassemble="$f" 2>/dev/null | grep -P '^\s*[0-9a-f]+:\t(stm|strd)' | grep -vP '\t(stm)\w*(\.w)?\tsp|\[sp')
  printf '%-28s %s\n' "$f" "$(echo -n "$n" | grep -c . )"
  [ -n "$n" ] && echo "$n" | sed 's/^/    /' && summe=$((summe + 1))
done
[ "$summe" -eq 0 ]
