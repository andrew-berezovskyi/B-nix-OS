#!/usr/bin/env bash
set -euo pipefail

mkdir -p build
make disk_image
set +e
timeout 60s qemu-system-i386 \
  -m 256M \
  -cdrom b-nix.iso \
  -drive file=c_drive.img,format=raw,if=ide \
  -vga std -display none -serial file:build/qemu-serial.log \
  -no-reboot -no-shutdown
status=$?
set -e

cat build/qemu-serial.log || true
if ! grep -q "BNIX_BOOT_OK" build/qemu-serial.log; then
  echo "B-nix did not reach the boot-ready marker (qemu status: $status)." >&2
  exit 1
fi
