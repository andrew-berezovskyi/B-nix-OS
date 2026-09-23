# B-nix applications

Applications in this directory are freestanding ELF32 programs. They are built
separately from the kernel and communicate through the small B-nix system-call
ABI in `apps/libc.h`.

## Aurora Calculator

`calc.bin` is the first distributable B-nix application. It supports repeated
integer calculations with `+`, `-`, `*`, `/`, and `%`, handles division
by zero, and exits with `q`.

Build it with:

```sh
make calc.bin
```

Run it from the B-nix terminal after the binary has been copied to the B-nix
filesystem:

```text
run calc.bin
```

The GitHub Actions `b-nix-build` artifact publishes `calc.bin` independently
of the bootable ISO. This keeps the application format modular: future package
installation can place compatible ELF32 binaries on disk without relinking the
kernel.

## Current ABI limits

The public app ABI currently provides terminal input/output and process exit.
Filesystem wrappers exist at kernel level but still need a hardened user-memory
boundary before they are considered a stable third-party SDK. Graphical
applications will require a versioned window/event API; they must not call the
kernel compositor directly.
