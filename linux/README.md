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

The resulting QEMU launcher and disk image are placed in:

```text
build/linux-output/images/
```

Run the generated `start-qemu.sh` script from that directory. The initial
image requests DHCP on `eth0` and includes `ip`, CA certificates, and
`curl` so networking is testable rather than represented by placeholder UI.

## Ownership boundary

Linux supplies the kernel and drivers. Buildroot supplies the reproducible
base userspace construction. B-nix owns the image policy, branding, desktop,
settings, applications, update path, QA, and any explicitly maintained custom
modules.

See `docs/ADR-001-linux-hybrid.md` for the architecture decision and licensing
boundary.

## Near-term milestones

1. deterministic x86_64 QEMU boot and serial readiness check;
2. virtio disk/network validation and persistent filesystem;
3. B-nix init/session service and system settings backend;
4. Wayland compositor/session with resolution-independent B-nix UI;
5. signed application bundles and update metadata;
6. hardware profiles beyond the QEMU reference platform.
