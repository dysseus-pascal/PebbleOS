"""Firmware-Wechsel im QEMU-Abbild: die System-Ressourcen (Bank 0) der neuen Firmware ins Flash
der alten, alles andere (Dateisystem, Debug-DB, Zeitzonen) bleibt. Wie ein Update auf der Uhr,
das die Ressourcen mitschreibt. Layout: fw/flash_region/flash_region_qemu.h (b26 und V2 gleich).

Aufruf: python3 tauschen.py <flash alt> <flash neu (aus dem Bau)> <ausgabe>
"""
import sys

BANK0 = 0x620000
BANK0_GROESSE = 0x200000

alt = bytearray(open(sys.argv[1], 'rb').read())
neu = open(sys.argv[2], 'rb').read()
assert len(alt) == len(neu) == 0x2000000, 'kein 32-MB-Abbild'
alt[BANK0:BANK0 + BANK0_GROESSE] = neu[BANK0:BANK0 + BANK0_GROESSE]
open(sys.argv[3], 'wb').write(alt)
print(f'Bank 0 ({BANK0_GROESSE // 1024} KB ab 0x{BANK0:x}) getauscht -> {sys.argv[3]}')
