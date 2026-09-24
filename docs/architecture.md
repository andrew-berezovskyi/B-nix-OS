# B-nix OS architecture

## Boot sequence

1. GRUB loads the Multiboot kernel and six named modules.
2. `core/boot.asm` establishes a bootstrap stack and enters `kernel_main`.
3. The kernel initializes serial output, GDT, IDT/exception gates, PMM, heap and paging.
4. VBE, input, PIT, filesystem and desktop services are initialized.
5. The scheduler starts the GUI and background kernel tasks.
6. A `BNIX_BOOT_OK` marker is written to COM1 for automated smoke tests.

## Trust model

The current tree is a transitional kernel. Kernel tasks and ELF applications still execute with ring-0 privileges. The next architecture milestone is ring-3 execution with TSS-managed kernel stacks, validated user pointers, per-process page tables and resource teardown.

## Subsystems

- **Physical memory:** bitmap allocator; bootstrap memory below 32 MiB is reserved.
- **Virtual memory:** paging is enabled early; process isolation is still incomplete.
- **Scheduling:** round-robin PIT scheduler with sleeping and keyboard-wait states.
- **Storage:** primary-master ATA PIO and a compact inode/block filesystem.
- **Graphics:** VBE framebuffer with software backbuffer, glyph/image caches and window buffers.
- **Userspace:** ELF32 loader plus a minimal syscall-oriented libc.

## Engineering invariants

- Every fatal CPU exception must produce a serial panic record.
- Interrupts stay disabled until interrupt controllers and devices are initialized.
- No generated image or object file belongs in Git.
- Every merged change must pass build, Multiboot validation and QEMU smoke boot.
