#!/usr/bin/env bash
set -euo pipefail

image_dir="${1:-build/linux-output/images}"
serial_log="build/qemu-linux-serial.log"
monitor_socket="build/qemu-linux-monitor.sock"
screen_dump="build/qemu-linux-screen.ppm"
mkdir -p build
rm -f "$serial_log" "$monitor_socket" "$screen_dump"
: > "$serial_log"

kernel="$image_dir/bzImage"
rootfs="$image_dir/rootfs.ext4"
[[ -f "$kernel" ]] || { echo "Missing Linux kernel: $kernel" >&2; exit 1; }
[[ -f "$rootfs" ]] || { echo "Missing Linux rootfs: $rootfs" >&2; exit 1; }

qemu-system-x86_64 \
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
    -monitor "unix:$monitor_socket,server=on,wait=off" \
    -no-reboot \
    -no-shutdown &
qemu_pid=$!

cleanup() {
    kill "$qemu_pid" 2>/dev/null || true
    wait "$qemu_pid" 2>/dev/null || true
    rm -f "$monitor_socket"
}
trap cleanup EXIT

ready=0
for _ in $(seq 1 120); do
    if ! kill -0 "$qemu_pid" 2>/dev/null; then
        echo "QEMU exited before readiness markers were observed" >&2
        tail -n 160 "$serial_log" >&2
        exit 1
    fi
    if grep -Fq '[BNIX-LINUX] BNIX_LINUX_BOOT_OK' "$serial_log" &&
       grep -Fq '[BNIX-LINUX] network ready' "$serial_log" &&
       grep -Fq '[BNIX-GUI] shell ready' "$serial_log"; then
        ready=1
        break
    fi
    sleep 1
done

if [[ $ready -ne 1 ]]; then
    echo "B-nix did not reach boot, network, and graphical readiness" >&2
    tail -n 160 "$serial_log" >&2
    exit 1
fi
if grep -Eq 'Kernel panic|BUG:|Oops:' "$serial_log"; then
    echo "Linux boot reported a fatal kernel condition" >&2
    tail -n 160 "$serial_log" >&2
    exit 1
fi

sleep 2
[[ -S "$monitor_socket" ]] || { echo "QEMU monitor socket is unavailable" >&2; exit 1; }
printf 'screendump %s\n' "$screen_dump" | socat - "UNIX-CONNECT:$monitor_socket"
[[ -s "$screen_dump" ]] || { echo "B-nix graphical screenshot was not created" >&2; exit 1; }

echo "B-nix Linux boot, network, and graphical shell smoke test passed."
echo "Graphical checkpoint: $screen_dump"
