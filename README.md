# B-nix OS

B-nix OS is an experimental 32-bit x86 operating system with a freestanding C kernel, GRUB/Multiboot boot path, a graphical desktop, a small persistent filesystem, and ELF applications.

> Current target: QEMU `qemu-system-i386`. Real hardware, UEFI, SMP, networking, audio and USB are not yet supported.

## Features

- GDT, IDT, CPU exception diagnostics and serial panic output.
- PIT-driven preemptive kernel tasks.
- Keyboard, PS/2 mouse, RTC, ATA PIO and VBE framebuffer drivers.
- Physical/virtual memory management and a kernel heap.
- Persistent educational filesystem, shell and file manager.
- Login screen, compositor, movable windows and a calculator app.

## Desktop experience

The current visual milestone introduces an original B-nix aurora wallpaper, a compact top system bar, a centered application dock, macOS-inspired traffic-light window controls, a redesigned login card, and a consistent light/dark palette across the terminal, file manager and viewer.

Every CI boot captures `build/qemu-screen.ppm` alongside the ISO and serial log, so visual regressions can be reviewed from the workflow artifact.

## Reproducible build

On Ubuntu/Debian:

```bash
sudo apt-get install gcc-multilib binutils nasm grub-pc-bin grub-common xorriso qemu-system-x86
make clean all check
make run
```

Headless smoke test:

```bash
bash scripts/qemu-smoke.sh
```

The CI workflow builds the ISO, validates the Multiboot header, boots it in QEMU and waits for the `BNIX_BOOT_OK` serial marker.

Login for the current demo UI: `admin` / `1234`. This is a temporary UI gate, not a security boundary.

## Repository layout

- `core/` — boot, descriptor tables, exceptions, syscalls and kernel entry.
- `drivers/` — serial, input, timer, ATA, RTC and framebuffer drivers.
- `mm/` — physical memory, paging and kernel heap.
- `fs/` — filesystem and shell.
- `gui/` — desktop, renderer, login and window manager.
- `apps/` — ELF applications and the small userspace support library.
- `docs/` — architecture and development roadmap.

Generated binaries, ISO images, virtual disks and object files are intentionally not committed. Download release images from GitHub Actions or Releases.
