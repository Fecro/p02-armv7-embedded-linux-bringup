#!/usr/bin/env bash
set -euo pipefail

PROJ_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CROSS_COMPILE="${CROSS_COMPILE:-arm-linux-gnueabihf-}"

echo "=== [Step 5/7] Auditing (ASan/UBSan) & Cross-Compiling C23 sysmon_c23 ==="
mkdir -p "${PROJ_ROOT}/rootfs/usr/bin"

cd "${PROJ_ROOT}/user/sysmon_c23"

echo "-> Running Host AddressSanitizer + UndefinedBehaviorSanitizer check..."
make CROSS_COMPILE="" sanitize

echo "-> Cross-compiling static ARMv7 binary and installing into RootFS..."
make CROSS_COMPILE="${CROSS_COMPILE}" PROJ_ROOT="${PROJ_ROOT}" install-rootfs

file "${PROJ_ROOT}/rootfs/usr/bin/sysmon_c23"
echo "[OK] sysmon_c23 installed at: ${PROJ_ROOT}/rootfs/usr/bin/sysmon_c23"
