#!/usr/bin/env bash
set -euo pipefail

mkdir -p build
make disk_image
rm -f build/qemu-serial.log build/qemu-monitor.sock build/qemu-screen.ppm

set +e
timeout 60s qemu-system-i386 \
  -m 256M \
  -cdrom b-nix.iso \
  -drive file=c_drive.img,format=raw,if=ide \
  -vga std -display none -serial file:build/qemu-serial.log \
  -monitor unix:build/qemu-monitor.sock,server=on,wait=off \
  -no-reboot -no-shutdown &
qemu_pid=$!
set -e

for _ in $(seq 1 55); do
  if grep -Fq "[BNIX] BNIX_BOOT_OK" build/qemu-serial.log 2>/dev/null; then
    sleep 2
    if [ -S build/qemu-monitor.sock ]; then
      printf 'screendump build/qemu-screen.ppm\n' | socat - UNIX-CONNECT:build/qemu-monitor.sock || true
    fi
    break
  fi
  sleep 1
done

set +e
wait "$qemu_pid"
status=$?
set -e

cat build/qemu-serial.log || true
if grep -Fq "[BNIX:PANIC]" build/qemu-serial.log; then
  echo "B-nix reported a kernel panic during boot." >&2
  exit 1
fi
if ! grep -Fq "[BNIX] BNIX_BOOT_OK" build/qemu-serial.log; then
  echo "B-nix did not reach the boot-ready marker (qemu status: $status)." >&2
  exit 1
fi
