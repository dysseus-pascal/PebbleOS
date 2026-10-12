#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 dysseus-pascal
# SPDX-License-Identifier: Apache-2.0
"""Boulder: Die Health-App ohne Systemrechte, geprueft an ihren Objektdateien (Schritt 3, 3.7.5, T2.2).

Aufruf: pruefe_ohne_rechte.py <bauordner> [<repo>]

Liest aus <bauordner>/build.ninja, welche Objekte von fw/apps/system/boulder_health/ in
pebbleos.elf gelinkt werden, und prueft jedes. Rot (Rueckgabe 1), wenn
  - ein Objekt beschreibbaren Speicher anlegt: ein Symbol oder Bytes in einem beschreibbaren
    Abschnitt (.data, .bss, ...) oder ein COMMON-Symbol. Ohne Systemrechte laege das in
    .kernel_bss, das die App nicht schreiben darf (BP 6.2);
  - die App ein Symbol ruft oder anspricht, das sie nicht selbst definiert und das nicht erlaubt
    ist. Erlaubt sind die Applib nach implName aus tools/generate_native_sdk/exported_symbols.json
    (was SDK-Apps tatsaechlich rufen), die Syscalls des Abbilds, libc ohne Zustand und die feste
    Liste HELFER. Ein Syscall ist eine Funktion aus DEFINE_SYSCALL: in pebbleos.elf zwischen
    __syscall_text_start__ und __syscall_text_end__ (fw/linker/sections/text.ld). Ein sys_*
    ausserhalb ist eine Kernfunktion, die ohne Rechte faellt (etwa
    sys_msg_protocol_msg_callback);
  - die App etwas von der roten Liste (L5) ruft, auch wenn es sonst erlaubt waere;
  - die App eine SDK-Funktion ruft, die fuer eine App mit System-ID Kernel-RAM liest (SYSTEM_ID);
  - keine Objektdatei der App gelinkt ist oder keine ein Symbol definiert.
"""
import json
import pathlib
import re
import struct
import sys

APP_ORDNER = '/fw__apps__system.dir/boulder_health/'

# Helfer ausserhalb des SDK, die eine eingebaute App ohne Systemrechte rufen darf (Spezifikation
# 3.7.5). Jeder laeuft ohne Syscall in der App-Task und schreibt nur App-Speicher.
HELFER = {
    # Zeiger auf den Zustand der App; der Zeiger selbst liegt im App-State (app_state.c:139,
    # :403-409), den die App-Task schreiben darf.
    'app_state_set_user_data',
    'app_state_get_user_data',
    # Initialisieren eingebetteter Fenster und Ebenen im Heap der App; das SDK kennt nur
    # *_create, das dasselbe nach einem malloc tut.
    'window_init',
    'layer_init',
    'text_layer_init_with_parameters',
    # malloc im Heap der App, das bei Mangel die App beendet statt NULL zu liefern
    # (kernel/pbl_malloc.h:37-40).
    'app_malloc_check',
    'app_zalloc_check',
}

# libc ohne Zustand: rechnen nur auf ihren Argumenten (kein errno, kein statischer Puffer, keine
# Locale). Dazu die Laufzeit-Helfer der ARM-EABI fuer Division, 64-Bit und Speicherblocke.
LIBC = {
    'memcpy', 'memmove', 'memset', 'memcmp', 'memchr',
    'strlen', 'strnlen', 'strcmp', 'strncmp', 'strcpy', 'strncpy', 'strcat', 'strncat',
    'strchr', 'strrchr', 'strstr',
    'snprintf', 'vsnprintf',
    'abs', 'labs',
}
# Laufzeit-Helfer, die der Compiler selbst einsetzt: die der ARM-EABI (Division, 64 Bit, Gleitkomma,
# Speicherbloecke) und die Bitzaehler von libgcc. Ausgenommen sind die wenigen mit Zustand
# (Thread-Zeiger, errno, atexit, Ausnahmen).
EABI = re.compile(r'^(__aeabi_\w+|__(popcount|clz|ctz|ffs)[sd]i2)$')
EABI_NEIN = re.compile(r'^__aeabi_(read_tp|errno_addr|atexit|unwind_cpp_\w*)$')

# Rote Liste (L5): Diese Aufrufe warten auf KernelBG oder bringen Werte vom Telefon. Sie sind rot,
# auch wenn sie im SDK stehen oder Syscalls sind.
ROT = re.compile(r'^(sys_activity_get_minute_history|health_service_get_minute_history|'
                 r'(sys_)?persist_.*|(sys_)?app_message_.*)$')

# Im SDK, aber fuer eine App mit System-ID nicht ohne Systemrechte: Pebbles Applib nimmt dann die
# Textgroesse des Systems und liest sie direkt aus Kernel-RAM (menu_layer_system_cells.c:69-78,
# s_system_text_size in shell/normal/prefs.c:1909-1911). Gefunden von der Probe in AP1, wo die App
# daran endete. Dasselbe gilt fuer die Zeilenhoehe einer MenuLayer ohne get_cell_height
# (menu_layer.c:490-502); das sieht keine Objektdatei, nur die Probe.
SYSTEM_ID = {'menu_cell_basic_draw'}

SHF_WRITE, SHF_ALLOC = 0x1, 0x2
SHT_SYMTAB = 2
SHN_UNDEF, SHN_COMMON = 0, 0xfff2
STT_FUNC, STT_SECTION, STT_FILE = 2, 3, 4


def sdk_namen(repo):
    """implName (sonst name) jeder SDK-Funktion, die nicht entfernt ist."""
    daten = json.loads((pathlib.Path(repo) / 'tools/generate_native_sdk/exported_symbols.json')
                       .read_text())
    namen = set()

    def gehe(x):
        if isinstance(x, dict):
            if x.get('type') == 'function' and not x.get('removed'):
                namen.add(x.get('implName', x['name']))
            for v in x.values():
                gehe(v)
        elif isinstance(x, list):
            for v in x:
                gehe(v)
    gehe(daten)
    return namen


def lies_elf(pfad, mit_wert=False):
    """(Abschnitte, Symbole) einer ELF32-Datei (little endian).

    Abschnitte: [(name, flags, groesse)]; Symbole: [(name, abschnitt_index, typ, global)], mit
    `mit_wert` dazu der Wert (die Adresse). Eigener Leser statt readelf: Das Ergebnis haengt nicht am
    Format eines Werkzeugs."""
    b = pathlib.Path(pfad).read_bytes()
    if b[:4] != b'\x7fELF' or b[4] != 1 or b[5] != 1:
        raise ValueError(f'{pfad}: keine ELF32-Datei (little endian)')
    shoff, = struct.unpack_from('<I', b, 0x20)
    shentsize, shnum, shstrndx = struct.unpack_from('<HHH', b, 0x2e)
    kopf = [struct.unpack_from('<10I', b, shoff + i * shentsize) for i in range(shnum)]

    def text(tab, ofs):
        start = kopf[tab][4] + ofs
        return b[start:b.index(b'\0', start)].decode()
    abschnitte = [(text(shstrndx, k[0]), k[2], k[5]) for k in kopf]
    symbole = []
    for k in kopf:
        if k[1] != SHT_SYMTAB:
            continue
        for i in range(k[5] // 16):
            name, wert, _groesse, info, _other, ndx = struct.unpack_from('<IIIBBH', b, k[4] + i * 16)
            eintrag = (text(k[6], name) if name else '', ndx, info & 0xf, (info >> 4) != 0)
            symbole.append(eintrag + (wert,) if mit_wert else eintrag)
    return abschnitte, symbole


def syscalls_im_abbild(elf):
    """Die Namen der echten Syscalls: Funktionen in Abschnitten .syscall_text.<name> (eine
    Objektdatei), im gelinkten Abbild zwischen __syscall_text_start__ und __syscall_text_end__."""
    abschnitte, symbole = lies_elf(elf, mit_wert=True)
    werte = {name: wert for name, _ndx, _typ, _glob, wert in symbole if name}
    start, ende = werte.get('__syscall_text_start__'), werte.get('__syscall_text_end__')
    namen = set()
    for name, ndx, typ, _glob, wert in symbole:
        if typ != STT_FUNC or not name:
            continue
        im_abschnitt = ndx < len(abschnitte) and abschnitte[ndx][0].startswith('.syscall_text.')
        # Thumb: Das unterste Bit der Adresse einer Funktion ist gesetzt.
        im_bereich = start is not None and ende is not None and start <= (wert & ~1) < ende
        if im_abschnitt or im_bereich:
            namen.add(name)
    return namen


def erlaubt(name, sdk, syscalls):
    if ROT.match(name):
        return False, 'rote Liste (L5)'
    if name in SYSTEM_ID:
        return False, 'liest mit System-ID Kernel-RAM'
    if name.startswith('sys_') and name not in syscalls:
        return False, 'kein Syscall'
    if name in sdk or name in syscalls or name in HELFER or name in LIBC or (
            EABI.match(name) and not EABI_NEIN.match(name)):
        return True, ''
    return False, 'nicht erlaubt'


def pruefe(objekte, sdk, syscalls):
    """Befunde je Objekt: {pfad: [meldung, ...]}; dazu, ob irgendein Objekt Symbole definiert."""
    befunde, definiert, offen = {}, set(), {}
    for o in objekte:
        abschnitte, symbole = lies_elf(o)
        meldungen = befunde.setdefault(o, [])
        # Jeder beschreibbare Abschnitt mit Inhalt ist rot, mit den Namen darin, sonst ohne Namen
        # (Bytes, die etwa der Assembler oder ein anonymes Objekt anlegt).
        beschreibbar = {i: [] for i, (_, flags, groesse) in enumerate(abschnitte)
                        if flags & SHF_ALLOC and flags & SHF_WRITE and groesse}
        for name, ndx, typ, glob in symbole:
            if not name or typ in (STT_SECTION, STT_FILE):
                continue
            if ndx == SHN_COMMON:
                meldungen.append(f'beschreibbar: COMMON-Symbol {name}')
            elif ndx == SHN_UNDEF:
                offen.setdefault(o, set()).add(name)
            else:
                if ndx in beschreibbar:
                    beschreibbar[ndx].append(name)
                if glob:
                    definiert.add(name)
        for ndx, namen in beschreibbar.items():
            name, _, groesse = abschnitte[ndx]
            meldungen.append(f'beschreibbar: {", ".join(namen) or "ohne Namen"} in {name} ({groesse} B)')
    for o, namen in offen.items():
        for name in sorted(namen - definiert):
            ok, grund = erlaubt(name, sdk, syscalls)
            if not ok:
                befunde[o].append(f'{grund}: {name}')
    return befunde, bool(definiert) or any(offen.values())


def objekte_im_bau(bau):
    """Die Objekte der App, die in pebbleos.elf gelinkt werden (aus build.ninja)."""
    for zeile in (pathlib.Path(bau) / 'build.ninja').read_text().splitlines():
        if zeile.startswith('build pebbleos.elf:'):
            return sorted(str(pathlib.Path(bau) / t) for t in zeile.split()
                          if APP_ORDNER in t and t.endswith('.obj'))
    return []


def main(argv):
    if len(argv) not in (2, 3):
        print(__doc__)
        return 2
    bau = argv[1]
    repo = argv[2] if len(argv) == 3 else pathlib.Path(__file__).resolve().parents[2]
    objekte = objekte_im_bau(bau)
    if not objekte:
        print(f'ROT {bau}: keine Objektdatei der Health-App in pebbleos.elf gelinkt')
        return 1
    elf = pathlib.Path(bau) / 'pebbleos.elf'
    syscalls = syscalls_im_abbild(elf) if elf.exists() else set()
    if not syscalls:
        print(f'ROT {bau}: keine Syscalls in pebbleos.elf gefunden')
        return 1
    befunde, gebaut = pruefe(objekte, sdk_namen(repo), syscalls)
    if not gebaut:
        print(f'ROT {bau}: die Objekte der Health-App sind leer (Schalter aus?)')
        return 1
    rot = 0
    for o in objekte:
        kurz = o.split(APP_ORDNER)[-1]
        if befunde[o]:
            rot = 1
            for m in befunde[o]:
                print(f'ROT {kurz}: {m}')
        else:
            print(f'ok  {kurz}')
    return rot


if __name__ == '__main__':
    sys.exit(main(sys.argv))
