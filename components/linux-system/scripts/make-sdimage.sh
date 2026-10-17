#!/usr/bin/env bash
set -euo pipefail
export PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root"
mkdir -p build/images logs
for artifact in build/uboot/u-boot-sunxi-with-spl.bin build/uboot/tools/mkimage build/kernel/arch/arm/boot/zImage build/kernel/arch/arm/boot/dts/suniv-f1c100s-generic.dtb build/rootfs/images/rootfs.ext4 build/rootfs/host/bin/genimage; do
    test -f "$artifact" || { echo "Missing artifact: $artifact" >&2; exit 1; }
done
# Match the boot.cmd load windows (16-22 MiB kernel, 22-23 MiB FDT).
[ "$(stat -c %s build/kernel/arch/arm/boot/zImage)" -lt 6291456 ] || { echo "zImage exceeds load window" >&2; exit 1; }
[ "$(stat -c %s build/kernel/arch/arm/boot/dts/suniv-f1c100s-generic.dtb)" -lt 1048576 ] || { echo "DTB exceeds load window" >&2; exit 1; }
if [ -f build/kernel/arch/arm/boot/Image ]; then
    [ "$(stat -c %s build/kernel/arch/arm/boot/Image)" -lt 16744448 ] || { echo "Uncompressed kernel exceeds reserved destination" >&2; exit 1; }
fi
cp build/uboot/u-boot-sunxi-with-spl.bin build/images/
cp build/kernel/arch/arm/boot/zImage build/images/
cp build/kernel/arch/arm/boot/dts/suniv-f1c100s-generic.dtb build/images/
cp --sparse=always build/rootfs/images/rootfs.ext4 build/images/rootfs.ext4
build/uboot/tools/mkimage -A arm -O linux -T script -C none -n 'Cherry Pi SD serial boot' -d board/cherrypi/boot.cmd build/images/boot.scr
export PATH="$root/build/rootfs/host/bin:$root/build/rootfs/host/sbin:$PATH"
genimage --rootpath "$root/build/rootfs/target" --tmppath "$root/build/sdimage-tmp" --inputpath "$root/build/images" --outputpath "$root/build/images" --config "$root/board/cherrypi/genimage.cfg" 2>&1 | tee logs/sdimage-build.log
(cd build/images && sha256sum sdcard.img > sdcard.img.sha256)
ls -lh build/images/sdcard.img
