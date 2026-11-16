#!/usr/bin/env bash
set -euo pipefail

echo "=== [Step 1/7] Installing Host Toolchain, QEMU & Packaging Dependencies ==="
sudo apt-get update
sudo apt-get install -y \
  build-essential git wget curl bc bison flex libssl-dev \
  libncurses-dev libelf-dev dwarves device-tree-compiler \
  u-boot-tools cpio gzip bzip2 xz-utils fdisk dosfstools mtools \
  qemu-system-arm gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf \
  gdb-multiarch valgrind cppcheck

echo "[OK] All host dependencies verified."
