#!/usr/bin/env bash
set -euo pipefail
export PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root"
mkdir -p logs
mkdir -p build/rootfs
if [ ! -f build/rootfs/.config ]; then
    cp configs/rootfs-verified.config build/rootfs/.config
fi
sed -i "s|@SYSTEM_ROOT@|$root|g" build/rootfs/.config

make -C buildroot O="$root/build/rootfs" -j"${JOBS:-4}" 2>&1 | tee logs/rootfs-build-latest.log
