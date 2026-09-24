# B-nix Linux platform

This directory is the production-platform path for B-nix OS. It is a
Buildroot `br2-external` tree: upstream Buildroot and Linux sources are fetched
into the ignored `build/` directory, while B-nix configuration and owned
components remain in this repository.

The original freestanding kernel at the repository root remains available as a
legacy/reference target.

## Build on Linux or WSL2

Install the standard Buildroot host prerequisites, then run:

```sh
bash linux/build.sh
```

The script checks out the protected Buildroot `2026.08` release commit
`d5180309b1b66ef3b8eaccca70ad69be8e0729a1`, applies
`b_nix_x86_64_defconfig`, and writes generated files only below `build/`.

The resulting disk and kernel images are placed in:

```text
build/linux-output/images/
```

## Run the graphical reference image

```sh
qemu-system-x86_64 \
  -M pc -cpu max -m 512M \
  -kernel build/linux-output/images/bzImage \
  -append "rootwait root=/dev/vda console=ttyS0" \
  -drive file=build/linux-output/images/rootfs.ext4,if=virtio,format=raw \
  -netdev user,id=net0 \
  -device virtio-net-pci,netdev=net0 \
  -device virtio-vga \
  -serial stdio
```

The image requests DHCP on `eth0`, includes `ip`, CA certificates, and
`curl`, and starts the B-nix-owned `bnix-shell` session on `/dev/fb0`.
The first shell milestone provides responsive desktop geometry, branded
top chrome, a status clock, an Aurora welcome surface, and a dock. It is an
owned userspace component rather than kernel-resident UI.

The CI smoke test requires all of these markers:

```text
[BNIX-LINUX] network ready
[BNIX-LINUX] BNIX_LINUX_BOOT_OK
[BNIX-GUI] shell ready
```

## Ownership boundary

Linux supplies the kernel and drivers. Buildroot supplies the reproducible
base userspace construction. B-nix owns the image policy, branding, desktop,
settings, applications, update path, QA, and any explicitly maintained custom
modules.

See `docs/ADR-001-linux-hybrid.md` for the architecture decision and licensing
boundary.

## Near-term milestones

1. replace the framebuffer preview with a Wayland/DRM compositor session;
2. add real input, focus, window lifecycle, and application launching;
3. implement settings and file-manager services against Linux APIs;
4. add persistent user data and signed application bundles;
5. create deterministic graphical screenshots in CI;
6. add hardware profiles beyond the QEMU reference platform.
