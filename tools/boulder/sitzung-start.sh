#!/bin/bash
# SPDX-FileCopyrightText: 2026 dysseus-pascal
# SPDX-License-Identifier: Apache-2.0
#
# Boulder: Bauumgebung fuer Claude-Code-Sitzungen in der Cloud (SessionStart-Hook).
# WARUM: Gebaut wird seit dem 12.10.2026 nur noch in der Cloud; jeder Container
# startet leer. Der Zustand nach dem Hook wird zwischengespeichert, darum darf er
# alles installieren und muss bei jedem Lauf schnell durchgehen, wenn schon alles da ist.
set -euo pipefail

if [ "${CLAUDE_CODE_REMOTE:-}" != "true" ]; then
    exit 0
fi

repo="${CLAUDE_PROJECT_DIR:-$(git -C "$(dirname "$0")" rev-parse --show-toplevel)}"
cd "$repo"

# SDK_VERSION nennt die Mindestfassung, aber das SDK 0.1.11 bringt GCC 15, und daran
# scheitert dieser Stand (-Werror=unterminated-string-initialization). Darum genau sie.
sdk_fassung="$(tr -d '[:space:]' < SDK_VERSION)"
sdk="$HOME/pebbleos-sdk-$sdk_fassung"

pakete=(bison clang flex gcc gcc-multilib gettext gperf libfreetype6-dev libglib2.0-dev
        libgtk-3-dev libncurses-dev librsvg2-bin make nodejs python3-dev python3-venv)
fehlend=()
for p in "${pakete[@]}"; do
    dpkg -s "$p" >/dev/null 2>&1 || fehlend+=("$p")
done
if [ "${#fehlend[@]}" -gt 0 ]; then
    apt-get update -qq
    DEBIAN_FRONTEND=noninteractive apt-get install -y -qq "${fehlend[@]}" >/dev/null
fi

if [ ! -x "$sdk/arm-none-eabi/bin/arm-none-eabi-gcc" ]; then
    tmp="$(mktemp -d)"
    curl -LsSf -o "$tmp/installer.sh" \
        "https://github.com/coredevices/PebbleOS-SDK/releases/download/v$sdk_fassung/pebbleos-sdk-installer.sh"
    sh "$tmp/installer.sh" --version "$sdk_fassung" --defaults
    rm -rf "$tmp"
fi

# Flach und einzeln, wie AGENTS.md es fuer Agenten-Arbeitsbaeume verlangt.
git config -f .gitmodules --get-regexp '^submodule\..*\.path$' | while read -r _ pfad; do
    if [ ! -e "$pfad/.git" ]; then
        git submodule update --init --depth 1 -- "$pfad"
    fi
done

if [ ! -x .venv/bin/pbl ]; then
    python3 -m venv .venv
fi
.venv/bin/pip install -q -r requirements.txt

if [ -n "${CLAUDE_ENV_FILE:-}" ]; then
    {
        echo "export PEBBLEOS_SDK_ROOT=\"$sdk\""
        echo "export VIRTUAL_ENV=\"$repo/.venv\""
        echo "export PATH=\"$repo/.venv/bin:$sdk/arm-none-eabi/bin:$sdk/qemu/bin:$sdk/sftool:\$PATH\""
    } >> "$CLAUDE_ENV_FILE"
fi
