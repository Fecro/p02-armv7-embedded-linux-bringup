#!/usr/bin/env bash
set -euo pipefail

PROJ_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MODE="${1:-uboot}"

echo "======================================================================"
echo " Launching QEMU ARM Versatile Express Cortex-A9 (Mode: ${MODE})"
echo " Tip: Press Ctrl+A, release, then press X to exit QEMU cleanly."
echo "======================================================================"

if [ "${MODE}" = "direct" ]; then
  exec qemu-system-arm \
    -machine vexpress-a9 \
    -cpu cortex-a9 \
    -smp 4 \
    -m 512M \
    -nographic \
    -audio none \
    -serial mon:stdio \
    -kernel "${PROJ_ROOT}/images/zImage" \
    -dtb "${PROJ_ROOT}/images/vexpress-v2p-ca9.dtb" \
    -initrd "${PROJ_ROOT}/images/initramfs.cpio.gz" \
    -append "console=ttyAMA0,115200 rdinit=/sbin/init loglevel=7"
else
  exec qemu-system-arm \
    -machine vexpress-a9 \
    -cpu cortex-a9 \
    -m 512M \
    -nographic \
    -audio none \
    -serial mon:stdio \
    -kernel "${PROJ_ROOT}/Bootloader/u-boot" \
    -drive if=sd,format=raw,file="${PROJ_ROOT}/images/sdcard.img"
fi
