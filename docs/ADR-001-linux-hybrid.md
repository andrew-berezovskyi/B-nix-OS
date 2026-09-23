# ADR-001: Linux-kernel-based B-nix platform

- Status: accepted
- Date: 2026-09-23

## Context

The original B-nix prototype is a useful freestanding 32-bit x86 learning kernel,
but it cannot realistically provide modern networking, broad hardware support,
graphics acceleration, process isolation, filesystems, and application
compatibility on the same schedule as the desktop product.

## Decision

B-nix will use an upstream Linux kernel for hardware enablement, networking,
memory management, processes, security primitives, and filesystems.

B-nix remains its own product through project-owned components:

- boot branding and first-run experience;
- Aurora desktop shell, compositor configuration, settings, and applications;
- B-nix service defaults, packaging policy, update channel, and release images;
- optional, narrowly scoped B-nix kernel modules when a userspace solution is
  insufficient;
- reproducible image configuration and automated boot/network tests.

The Linux source tree is not vendored into this repository. The build pins an
upstream Buildroot release/commit, which in turn pins a supported Linux kernel.
Project-specific configuration lives in a Buildroot `br2-external` tree under
`linux/`. This follows Buildroot's documented external customization model.

The existing freestanding kernel remains buildable as the legacy/reference
target until equivalent user-visible behavior exists on the Linux platform. It
will not be presented as the production B-nix runtime.

## Initial target

The first supported target is QEMU x86_64 with:

- virtio storage and network devices;
- DHCP networking;
- a persistent ext4 root filesystem;
- serial boot diagnostics and deterministic CI readiness markers;
- a minimal B-nix-branded userspace.

A graphical Wayland-based B-nix session follows after the boot, persistence,
network, and update foundations are reproducible.

## Consequences

This changes the product architecture rather than merely importing Linux code.
It accelerates real hardware and networking support while keeping the
user-facing system and policy under B-nix control.

Linux and Buildroot components retain their upstream licenses. B-nix-owned
components must keep clear license boundaries and publish corresponding source
where required.
