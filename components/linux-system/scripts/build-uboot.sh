#!/usr/bin/env bash
set -euo pipefail
export PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root"
mkdir -p build/uboot logs
# Pin the SD controller to MMC 0; the vendor tree only aliases disabled MMC 1.
if ! grep -Fq 'mmc0 = &mmc0;' sunxi_uboot/arch/arm/dts/suniv-f1c100s-generic.dts; then
    git -C sunxi_uboot apply --check "$root/patches/uboot-cherrypi-mmc0-alias.patch"
    git -C sunxi_uboot apply "$root/patches/uboot-cherrypi-mmc0-alias.patch"
fi
if [ ! -f build/uboot/.config ]; then
    cp configs/uboot-serial-baseline.config build/uboot/.config
fi
make -C sunxi_uboot O="$root/build/uboot" ARCH=arm CROSS_COMPILE=arm-linux-gnueabi- olddefconfig
make -C sunxi_uboot O="$root/build/uboot" ARCH=arm CROSS_COMPILE=arm-linux-gnueabi- -j"${JOBS:-4}" 2>&1 | tee logs/uboot-build-latest.log
