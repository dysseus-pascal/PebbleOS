# SPDX-FileCopyrightText: 2026 dysseus-pascal
# SPDX-License-Identifier: Apache-2.0

"""Boulder: die Einstellungen von TimeStyle Bedrock, wie Boulder sie schickt.

Schreibt den Wert "tsBedrock" (32 Byte, Aufbau in
src/fw/apps/watch/timestyle/ts_format.h) in die Uhreinstellungen-
Synchronisation des Emulators - derselbe Weg wie aus Boulder, also mit
Whitelist, Laengenpruefung und Neuzeichnen des laufenden Zifferblatts.
"""

import time

from pbl.feeds import BlobDb, Feed

SCHLUESSEL = b"tsBedrock\0"

# Die Vorgabe vom Foto des Nutzers (gemeinsamer Testvektor V0).
VORGABE = bytes.fromhex(
    "01ffc0ffc0000100070a04002e500000414c5400000000000000000000000000"
)

# Die Namen der Widgets, wie Boulder sie zeigt (Spezifikation 1.3).
WIDGETS = {
    "none": 0,
    "battery": 2,
    "timezone": 3,
    "date": 4,
    "seconds": 5,
    "week": 6,
    "weather": 7,
    "forecast": 8,
    "sleep": 9,
    "steps": 10,
    "beats": 11,
    "heartrate": 12,
    "uv": 13,
}


def _farbe(text):
    """Eine GColor8 als Hex ("f4") oder als #rrggbb, auf die 64 Farben gerundet."""
    text = text.lstrip("#")
    if len(text) == 6:
        r, g, b = (int(text[i : i + 2], 16) * 3 // 255 for i in (0, 2, 4))
        return 0xC0 | (r << 4) | (g << 2) | b
    return int(text, 16) | 0xC0


def _widget(text):
    if text.isdigit():
        return int(text)
    if text not in WIDGETS:
        raise ValueError(f"unknown widget {text!r}; pick from {', '.join(WIDGETS)}")
    return WIDGETS[text]


def kodieren(args):
    """Fassung 1 aus den Optionen; was fehlt, kommt aus der Vorgabe."""
    b = bytearray(VORGABE)
    for stelle, wert in ((1, args.time), (2, args.background), (3, args.sidebar),
                         (4, args.sidebar_text)):
        if wert is not None:
            b[stelle] = _farbe(wert)
    if args.language is not None:
        b[5] = args.language
    if getattr(args, "font", None) is not None:
        b[6] = args.font
    if args.hourly_vibe is not None:
        b[7] = args.hourly_vibe
    if args.widgets:
        teile = args.widgets.split(",")
        if len(teile) != 3:
            raise ValueError("--widgets wants three, top,middle,bottom")
        b[8:11] = bytes(_widget(t) for t in teile)
    if args.utc_offset is not None:
        b[11] = args.utc_offset & 0xFF
    if args.comma:
        b[12] = ord(",")

    flags_a = b[13]
    for bit, an in ((0, args.leading_zero), (1, args.bt_vibe), (2, args.left),
                     (3, args.large)):
        if an:
            flags_a |= 1 << bit
    if args.no_disconnect_icon:
        flags_a &= ~(1 << 4)
    if args.no_battery_pct:
        flags_a &= ~(1 << 6)
    if args.no_low_battery:
        flags_a |= 1 << 7
    b[13] = flags_a
    b[14] = (1 if args.distance else 0) | (2 if args.restful else 0)

    if args.tz_name is not None:
        name = args.tz_name.encode("utf-8")
        if len(name) > 7:
            raise ValueError("--tz-name holds at most 7 bytes")
        b[16:24] = name.ljust(8, b"\0")
    return bytes(b)


class TimeStyle(Feed):
    name = "timestyle"
    help = "Settings of TimeStyle Bedrock, as Boulder sends them"
    description = (
        "Write the tsBedrock watch setting: the defaults, changed by the "
        "options, or a raw value with --hex."
    )

    def add_arguments(self, parser):
        parser.add_argument("--hex", help="the raw value, 32 bytes as hex")
        parser.add_argument("--time", metavar="COLOR", help="time color (GColor8 hex or #rrggbb)")
        parser.add_argument("--background", metavar="COLOR", help="background color")
        parser.add_argument("--sidebar", metavar="COLOR", help="sidebar color")
        parser.add_argument("--sidebar-text", metavar="COLOR", help="sidebar text color")
        parser.add_argument("--language", type=int, help="language id")
        parser.add_argument(
            "--font", type=int,
            help="clock font (b24): 0 regular, 1 LECO, 2 bold, 3 bold hour, 4 bold minute",
        )
        parser.add_argument("--hourly-vibe", type=int, help="0 off, 1 hourly, 2 half-hourly")
        parser.add_argument(
            "--widgets", help="top,middle,bottom: " + ", ".join(WIDGETS) + " or ids"
        )
        parser.add_argument("--utc-offset", type=int, help="hours, -12 to 14")
        parser.add_argument("--tz-name", help="time zone label")
        parser.add_argument("--comma", action="store_true", help="decimal comma")
        parser.add_argument("--leading-zero", action="store_true")
        parser.add_argument("--bt-vibe", action="store_true")
        parser.add_argument("--left", action="store_true", help="sidebar on the left")
        parser.add_argument("--large", action="store_true", help="large sidebar")
        parser.add_argument("--no-disconnect-icon", action="store_true")
        parser.add_argument("--no-battery-pct", action="store_true")
        parser.add_argument("--no-low-battery", action="store_true")
        parser.add_argument("--distance", action="store_true", help="distance instead of steps")
        parser.add_argument("--restful", action="store_true", help="restful sleep only")
        parser.add_argument(
            "--plain-insert", action="store_true",
            help="plain insert without timestamp (older phones) instead of Boulder's way",
        )

    def run(self, args, watch, inf):
        wert = bytes.fromhex(args.hex) if args.hex else kodieren(args)
        # Absichtlich keine Pruefung der Laenge hier: so laesst sich auch
        # zeigen, dass die Firmware einen falsch langen Wert abweist.
        if args.plain_insert:
            watch.blobdb_insert(BlobDb.SETTINGS, SCHLUESSEL, wert)
        else:
            # Wie Boulder: mit Zeitstempel - die Firmware prueft auf diesem
            # Weg getrennt, also muss auch hier der echte Weg der Standard sein.
            watch.blobdb_insert_with_timestamp(BlobDb.SETTINGS, SCHLUESSEL, wert, int(time.time()))
        inf(f"tsBedrock = {wert.hex()}")
