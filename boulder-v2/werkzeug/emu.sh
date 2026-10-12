#!/bin/bash
# Prüfbau neu bauen (ohne das Flash-Abbild zu ersetzen) und QEMU neu starten.
set -e
cd /home/user/pebbleos-v2
. .venv/bin/activate; . ~/pebbleos-sdk-0.1.10/env.sh
B=build-pruef
[ -f $B/qemu.pid ] && kill $(cat $B/qemu.pid) 2>/dev/null || true
sleep 1
pbl build -b $B > /tmp/claude-0/-home-user/7e362f6d-34dc-553d-8345-69bd6588c9e5/scratchpad/bau-pruef.log 2>&1 || { grep -E 'error' /tmp/claude-0/-home-user/7e362f6d-34dc-553d-8345-69bd6588c9e5/scratchpad/bau-pruef.log | head; exit 1; }
cd $B
nohup /root/pebbleos-sdk-0.1.10/qemu/bin/qemu-pebble -rtc base=localtime -monitor unix:$PWD/qemu-mon.sock,server=on,wait=off -qmp unix:$PWD/qmp.sock,server=on,wait=off -serial file:uart1.log -serial tcp::12344,server=on,wait=off -serial tcp::12345,server=on,wait=off -machine pebble-emery -kernel $PWD/pebbleos.elf -drive if=mtd,format=raw,file=$PWD/qemu_spi_flash.bin -display none > qemu.out 2>&1 &
echo $! > qemu.pid
sleep 25
echo gestartet
