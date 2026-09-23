#!/usr/bin/env bash
set -euo pipefail

image_dir="${1:-build/linux-output/images}"
serial_log="build/qemu-linux-serial.log"
mkdir -p build
: > "$serial_log"

kernel="$image_dir/bzImage"
rootfs="$image_dir/rootfs.ext4"
[[ -f "$kernel" ]] || { echo "Missing Linux kernel: $kernel" >&2; exit 1; }
[[ -f "$rootfs" ]] || { echo "Missing Linux rootfs: $rootfs" >&2; exit 1; }

set +e
timeout 120s qemu-system-x86_64 \
    -M pc \
    -cpu max \
    -m 512M \
    -kernel "$kernel" \
    -append "rootwait root=/dev/vda console=ttyS0" \
    -drive "file=$rootfs,if=virtio,format=raw" \
    -netdev user,id=net0 \
    -device virtio-net-pci,netdev=net0 \
    -device virtio-vga \
    -display none \
    -serial "file:$serial_log" \
    -no-reboot \
    -no-shutdown
qemu_status=$?
set -e

if [[ $qemu_status -ne 0 && $qemu_status -ne 124 ]]; then
    echo "QEMU exited unexpectedly with status $qemu_status" >&2
    tail -n 120 "$serial_log" >&2
    exit 1
fi
if grep -Eq 'Kernel panic|BUG:|Oops:' "$serial_log"; then
    echo "Linux boot reported a fatal kernel condition" >&2
    tail -n 160 "$serial_log" >&2
    exit 1
fi
grep -Fq '[BNIX-LINUX] BNIX_LINUX_BOOT_OK' "$serial_log" || {
    echo "B-nix Linux readiness marker was not observed" >&2
    tail -n 160 "$serial_log" >&2
    exit 1
}
grep -Fq '[BNIX-LINUX] network ready' "$serial_log" || {
    echo "B-nix Linux did not acquire a DHCP address on eth0" >&2
    tail -n 160 "$serial_log" >&2
    exit 1
}
grep -Fq '[BNIX-GUI] shell ready' "$serial_log" || {
    echo "B-nix graphical shell did not acquire the framebuffer" >&2
    tail -n 160 "$serial_log" >&2
    exit 1
}

echo "B-nix Linux boot, network, and graphical shell smoke test passed."
