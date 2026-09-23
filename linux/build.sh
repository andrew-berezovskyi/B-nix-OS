#!/usr/bin/env bash
set -euo pipefail

readonly BUILDROOT_URL="https://gitlab.com/buildroot.org/buildroot.git"
readonly BUILDROOT_COMMIT="d5180309b1b66ef3b8eaccca70ad69be8e0729a1"
readonly SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
readonly SOURCE_DIR="$REPO_ROOT/build/linux-buildroot-src"
readonly OUTPUT_DIR="$REPO_ROOT/build/linux-output"

if [[ ! -d "$SOURCE_DIR/.git" ]]; then
    mkdir -p "$SOURCE_DIR"
    git -C "$SOURCE_DIR" init
    git -C "$SOURCE_DIR" remote add origin "$BUILDROOT_URL"
fi

current_remote="$(git -C "$SOURCE_DIR" remote get-url origin)"
if [[ "$current_remote" != "$BUILDROOT_URL" ]]; then
    echo "Refusing unexpected Buildroot remote: $current_remote" >&2
    exit 1
fi

if ! git -C "$SOURCE_DIR" cat-file -e "$BUILDROOT_COMMIT^{commit}" 2>/dev/null; then
    git -C "$SOURCE_DIR" fetch --depth 1 origin "$BUILDROOT_COMMIT"
fi
git -C "$SOURCE_DIR" checkout --detach "$BUILDROOT_COMMIT"

mkdir -p "$OUTPUT_DIR"
make -C "$SOURCE_DIR" O="$OUTPUT_DIR" BR2_EXTERNAL="$SCRIPT_DIR" b_nix_x86_64_defconfig
make -C "$SOURCE_DIR" O="$OUTPUT_DIR" BR2_EXTERNAL="$SCRIPT_DIR" "${@:-all}"

echo
echo "B-nix Linux images: $OUTPUT_DIR/images"
echo "Run with: $OUTPUT_DIR/images/start-qemu.sh"
