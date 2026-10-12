#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 dysseus-pascal
# SPDX-License-Identifier: Apache-2.0
"""Boulder: Gegenproben fuer pruefe_ohne_rechte.py (Schritt 3, T2.2; ctest).

Jede Probe ist ein kleines C-Stueck, das arm-none-eabi-gcc mit den Schaltern der Firmware
(-ffunction-sections -fdata-sections -Os, Cortex-M33) zu einer echten Objektdatei uebersetzt. Das
Pruefskript liest sie wie die Objekte der App. Die roten Proben muessen rot sein, und zwar aus dem
genannten Grund; die gruenen gruen. Der Compiler kommt aus dem PATH (SDK: env.sh); fehlt er, ist
der Test rot statt still uebersprungen.
"""
import contextlib
import io
import os
import pathlib
import shutil
import subprocess
import sys
import tempfile

sys.path.insert(0, str(pathlib.Path(__file__).parent))
import pruefe_ohne_rechte  # noqa: E402

REPO = pathlib.Path(__file__).resolve().parents[2]
GCC = 'arm-none-eabi-gcc'
SCHALTER = ['-c', '-x', 'c', '-std=c11', '-mthumb', '-mcpu=cortex-m33', '-Os', '-ffreestanding',
            '-ffunction-sections', '-fdata-sections']


def uebersetze(tmp, name, quelle, extra=()):
    pfad = os.path.join(tmp, name + '.c.obj')
    schalter = SCHALTER if '-x' not in extra else [s for s in SCHALTER if s not in ('-x', 'c')]
    subprocess.run([GCC, *schalter, *extra, '-o', pfad, '-'], input=quelle.encode(), check=True)
    return pfad


# Die Syscalls, die die Proben rufen; im Bau kommen sie aus pebbleos.elf (syscalls_im_abbild).
SYSCALLS = {'sys_activity_get_metric', 'sys_boulder_lage', 'sys_activity_get_minute_history'}


def befunde(*quellen, extra=()):
    """Alle Meldungen ueber ein oder mehrere Objekte (eine App aus mehreren Dateien)."""
    with tempfile.TemporaryDirectory() as tmp:
        objekte = [uebersetze(tmp, f'o{i}', q, extra) for i, q in enumerate(quellen)]
        alle, gebaut = pruefe_ohne_rechte.pruefe(objekte, SDK, SYSCALLS)
        return [m for o in objekte for m in alle[o]], gebaut


def rot(quelle, wort, extra=()):
    meldungen, _ = befunde(quelle, extra=extra)
    return any(wort in m for m in meldungen)


def gruen(*quellen, ruft=()):
    """Gruen, und die Objekte rufen wirklich `ruft` (sonst waere die Probe leer gruen)."""
    with tempfile.TemporaryDirectory() as tmp:
        objekte = [uebersetze(tmp, f'o{i}', q) for i, q in enumerate(quellen)]
        alle, gebaut = pruefe_ohne_rechte.pruefe(objekte, SDK, SYSCALLS)
        offen = {n for o in objekte for n, ndx, _, _ in pruefe_ohne_rechte.lies_elf(o)[1]
                 if n and ndx == pruefe_ohne_rechte.SHN_UNDEF}
    meldungen = [m for o in objekte for m in alle[o]]
    fehlt = set(ruft) - offen
    if meldungen or fehlt:
        print('      ' + '; '.join(meldungen + [f'ruft nicht: {n}' for n in sorted(fehlt)]))
    return gebaut and not meldungen and not fehlt


def aufruf(name, art='void', args='void'):
    """Eine Funktion, die nur `name` ruft (Deklaration ohne Kopf, wie sie die App sieht)."""
    return f'{art} {name}({args});\nvoid f(void) {{ (void){name}({"0" if args != "void" else ""}); }}\n'


SDK = None


def proben():
    # Spezifikation 3.7.5: ein static int, boulder_nacht_letzte, health_util_* und der
    # Minutenverlauf.
    yield 'static int', rot('static int zaehler;\nint f(void) { return ++zaehler; }\n', 'beschreibbar')
    yield 'boulder_nacht_letzte', rot(aufruf('boulder_nacht_letzte', 'int', 'void *'),
                                      'nicht erlaubt: boulder_nacht_letzte')
    yield 'health_util_format_hours_minutes_seconds', rot(
        aufruf('health_util_format_hours_minutes_seconds', 'int', 'char *'),
        'nicht erlaubt: health_util_format_hours_minutes_seconds')
    yield 'health_service_get_minute_history (im SDK, aber rot)', rot(
        aufruf('health_service_get_minute_history', 'int', 'void *'),
        'rote Liste (L5): health_service_get_minute_history')
    # Weitere Wege an Kern-Speicher und die uebrige rote Liste.
    yield 'static int mit Anfangswert (.data)', rot(
        'static int zaehler = 5;\nint f(void) { return ++zaehler; }\n', 'beschreibbar')
    yield 'globales int ohne static', rot('int zaehler;\nint f(void) { return ++zaehler; }\n',
                                         'beschreibbar')
    yield 'COMMON-Symbol (-fcommon)', rot('int zaehler;\nint f(void) { return ++zaehler; }\n',
                                         'COMMON', extra=['-fcommon'])
    yield 'beschreibbare Bytes ohne Namen (Assembler)', rot(
        '.data\n.word 5\n', 'beschreibbar: ohne Namen in .data (4 B)', extra=['-x', 'assembler'])
    yield 'static in einer Funktion', rot('int f(void) { static int n; return ++n; }\n', 'beschreibbar')
    yield 'sys_activity_get_minute_history (Syscall, aber rot)', rot(
        aufruf('sys_activity_get_minute_history', 'int', 'void *'), 'rote Liste (L5)')
    yield 'persist_read_int', rot(aufruf('persist_read_int', 'int', 'unsigned'), 'rote Liste (L5)')
    yield 'app_message_open', rot(aufruf('app_message_open', 'int', 'unsigned'), 'rote Liste (L5)')
    yield 'Kernfunktion kernel_malloc', rot(aufruf('kernel_malloc', 'void *', 'unsigned'),
                                            'nicht erlaubt: kernel_malloc')
    yield 'Kern-Daten (Linker-Symbol)', rot(
        'extern unsigned __kernel_main_stack_start__[];\n'
        'void f(void) { *(volatile unsigned *)__kernel_main_stack_start__ = 1; }\n',
        'nicht erlaubt: __kernel_main_stack_start__')
    yield 'Firmware-Name statt implName (graphics_fill_rect)', rot(
        aufruf('graphics_fill_rect', 'void', 'void *'), 'nicht erlaubt: graphics_fill_rect')
    yield 'Laufzeit-Helfer mit Zustand (__aeabi_read_tp)', rot(
        aufruf('__aeabi_read_tp', 'void *'), 'nicht erlaubt: __aeabi_read_tp')
    yield 'menu_cell_basic_draw (im SDK, mit System-ID Kernel-RAM)', rot(
        aufruf('menu_cell_basic_draw', 'void', 'void *'), 'liest mit System-ID Kernel-RAM')
    yield 'localtime (statischer Puffer)', rot(aufruf('localtime', 'void *', 'const void *'),
                                               'nicht erlaubt: localtime')
    # Gegenpruefung A9: sys_* ohne DEFINE_SYSCALL ist eine Kernfunktion (im obelix-Abbild 2 von 191).
    yield 'sys_* ohne Syscall (sys_msg_protocol_msg_callback)', rot(
        aufruf('sys_msg_protocol_msg_callback', 'void', 'void *'),
        'kein Syscall: sys_msg_protocol_msg_callback')

    # Gruen: was die App ruft, und Konstanten im Flash.
    yield 'erlaubt: SDK nach implName, sys_*, Helfer, libc, Konstanten', gruen(
        'struct W; struct W *window_create(void);\n'
        'void app_window_stack_push(struct W *, _Bool);\n'
        'void graphics_fill_round_rect_by_value(void *, long long, unsigned short, int);\n'
        'void *app_zalloc_check(unsigned);\nvoid task_free(void *);\n'
        'void app_state_set_user_data(void *);\nvoid window_init(struct W *, const char *);\n'
        '_Bool sys_activity_get_metric(int, unsigned, int *);\n_Bool sys_boulder_lage(void *);\n'
        'void app_log(unsigned char, const char *, int, const char *, ...);\n'
        'int snprintf(char *, unsigned, const char *, ...);\nvoid *memset(void *, int, unsigned);\n'
        'static const int s_tabelle[] = {3, 5, 7};\n'
        'unsigned f(unsigned teiler, int i) {\n'
        '  char text[16]; int wert = 0;\n'
        '  struct W *w = window_create(); window_init(w, "x"); app_window_stack_push(w, 1);\n'
        '  graphics_fill_round_rect_by_value(0, 0, 0, 0);\n'
        '  void *z = app_zalloc_check(8); app_state_set_user_data(z); memset(z, 0, 8); task_free(z);\n'
        '  sys_activity_get_metric(0, 1, &wert); sys_boulder_lage(z);\n'
        '  snprintf(text, sizeof(text), "%d", s_tabelle[i]);\n'
        '  app_log(1, "x", 1, "%s", text);\n'
        '  return (unsigned)((unsigned long long)wert * 1000u / teiler);\n'
        '}\n', ruft=('window_create', 'app_window_stack_push', 'graphics_fill_round_rect_by_value',
                     'app_zalloc_check', 'task_free', 'app_state_set_user_data', 'window_init',
                     'sys_activity_get_metric', 'sys_boulder_lage', 'app_log', 'snprintf', 'memset',
                     '__aeabi_uldivmod'))
    yield 'Aufruf in eine andere Datei der App', gruen(
        'int bh_hilfe(int x) { return x + 1; }\n',
        'int bh_hilfe(int);\nint f(void) { return bh_hilfe(2); }\n', ruft=('bh_hilfe',))
    leer, gebaut = befunde('/* Schalter aus */\n')
    yield 'leere Objekte (Schalter aus) sind nicht gruen', not leer and not gebaut
    yield from proben_bauordner()


# Statt des Abbilds eine Objektdatei mit den Syscalls in ihren Abschnitten, wie DEFINE_SYSCALL sie
# anlegt; syscalls_im_abbild liest beides.
ABBILD = ''.join(f'__attribute__((section(".syscall_text.{n}"))) void {n}(void) {{}}\n'
                 for n in sorted(SYSCALLS))


def bauordner(quelle, mit_app=True, abbild=ABBILD):
    """main() an einem Bauordner: build.ninja linkt das Objekt (oder nicht) in pebbleos.elf."""
    with tempfile.TemporaryDirectory() as tmp:
        if abbild is not None:
            os.replace(uebersetze(tmp, 'abbild', abbild), os.path.join(tmp, 'pebbleos.elf'))
        rel = 'fw/apps/system/CMakeFiles/fw__apps__system.dir/boulder_health'
        os.makedirs(os.path.join(tmp, rel))
        uebersetze(os.path.join(tmp, rel), 'x', quelle)
        # Ein zweites Objekt im Ordner, das nicht gelinkt wird (altes Objekt einer geloeschten
        # Datei): Es zaehlt nicht.
        uebersetze(os.path.join(tmp, rel), 'alt', aufruf('kernel_malloc', 'void *', 'unsigned'))
        app = f' {rel}/x.c.obj' if mit_app else ''
        with open(os.path.join(tmp, 'build.ninja'), 'w') as f:
            f.write(f'build pebbleos.elf: C_EXECUTABLE_LINKER fw/a.c.obj{app} fw/b.c.obj\n')
        with contextlib.redirect_stdout(io.StringIO()):
            return pruefe_ohne_rechte.main(['pruefe_ohne_rechte.py', tmp, str(REPO)])


def proben_bauordner():
    yield 'Bauordner: gelinktes Objekt gruen, altes zaehlt nicht', bauordner(
        'int health_app_get_info(void) { return 1; }\n') == 0
    yield 'Bauordner: gelinktes Objekt rot', bauordner(aufruf('boulder_nacht_letzte')) == 1
    yield 'Bauordner: App nicht gelinkt', bauordner('int x(void) { return 1; }\n', False) == 1
    yield 'Bauordner: Syscall aus dem Abbild gruen', bauordner(
        aufruf('sys_boulder_lage', 'int', 'void *')) == 0
    yield 'Bauordner: ohne Abbild rot', bauordner(
        aufruf('sys_boulder_lage', 'int', 'void *'), abbild=None) == 1
    yield 'Abbild: Syscalls zwischen den Marken', prv_abbild_marken()


def prv_abbild_marken():
    """Ein gelinktes Abbild: Was zwischen __syscall_text_start__ und __syscall_text_end__ liegt,
    ist ein Syscall, eine Funktion davor nicht."""
    quelle = ('void sys_kern(void) {}\n'
              '__attribute__((section(".syscall_text.sys_echt"))) void sys_echt(void) {}\n'
              'void _start(void) { sys_kern(); sys_echt(); }\n')
    skript = ('SECTIONS { .text : { *(.text*) __syscall_text_start__ = .; *(.syscall_text.*) '
              '__syscall_text_end__ = .; } }\n')
    with tempfile.TemporaryDirectory() as tmp:
        objekt = uebersetze(tmp, 'abbild', quelle)
        with open(os.path.join(tmp, 'abbild.ld'), 'w') as f:
            f.write(skript)
        elf = os.path.join(tmp, 'pebbleos.elf')
        subprocess.run([GCC, '-nostdlib', '-mthumb', '-mcpu=cortex-m33', '-T',
                        os.path.join(tmp, 'abbild.ld'), '-o', elf, objekt], check=True)
        namen = pruefe_ohne_rechte.syscalls_im_abbild(elf)
    return namen == {'sys_echt'}


def main():
    global SDK
    if not shutil.which(GCC):
        print(f'FEHLT {GCC} im PATH (SDK: env.sh)')
        return 1
    SDK = pruefe_ohne_rechte.sdk_namen(REPO)
    rot_ = 0
    for name, ok in proben():
        print(('ok    ' if ok else 'FEHLT ') + name)
        rot_ += not ok
    return 1 if rot_ else 0


if __name__ == '__main__':
    sys.exit(main())
