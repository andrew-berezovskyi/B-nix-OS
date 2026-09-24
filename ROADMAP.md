# Roadmap

## v0.1 — reproducible and diagnosable

- Reproducible build and CI boot test.
- Complete boot module manifest.
- Serial logging and CPU exception panic path.
- PMM initialization and reserved bootstrap region.
- Repository cleanup.

## v0.2 — memory and process isolation

- Multiboot memory-map based region discovery.
- Kernel higher-half or bounded kernel mapping.
- Ring-3 GDT entries, TSS and per-task kernel stacks.
- User pointer validation and page permissions.
- Complete task/address-space teardown.

## v0.3 — reliable storage

- ATA IDENTIFY, timeout and error propagation.
- Versioned on-disk superblock and consistency validation.
- Hierarchical directories, seek/stat and per-process descriptors.
- Persistence and corruption regression tests.

## v0.4 — desktop quality

- Pitch-aware dynamic framebuffer buffers.
- Resolution-independent layout and pointer bounds.
- Input event queue, clipping, z-order and improved damage tracking.
- Terminal scrolling, editor basics and filesystem UI completion.

## Later

Networking, audio, USB, SMP, x86_64 and UEFI are separate architecture projects and should start only after the 32-bit QEMU target is stable.
