# B-nix OS

An experimental 32-bit operating system with its own kernel, graphical desktop, and small applications. The build creates a GRUB-bootable ISO and runs it in QEMU with a separate virtual disk image.

## Implemented areas

- Kernel boot, GDT, IDT, and system calls.
- Keyboard, mouse, timer, RTC, ATA, and VBE drivers.
- Memory management, a filesystem, and a shell.
- Desktop, windows, login screen, and a calculator application.

This is an educational project intended for a virtual machine.

## Build and run

Use a Linux environment with `make`, `gcc` capable of 32-bit compilation, `nasm`, `ld`, `grub-mkrescue`, and `qemu-system-i386`. GRUB ISO creation may also require `xorriso`.

```bash
make             # build b-nix.iso
make run         # build, create the disk image if needed, and start QEMU
```

`make disk_image` creates a 10 MB `c_drive.img` if it does not already exist. `make clean` removes the build outputs listed in the Makefile but leaves `c_drive.img` in place, so disk state can survive subsequent runs.

## Repository layout

- `core/` — boot code and kernel.
- `drivers/` — device drivers.
- `mm/` — memory management.
- `fs/` — filesystem and shell.
- `gui/` — graphical interface.
- `apps/` — applications.
- `Makefile`, `linker.ld`, and `grub.cfg` — build and boot configuration.
