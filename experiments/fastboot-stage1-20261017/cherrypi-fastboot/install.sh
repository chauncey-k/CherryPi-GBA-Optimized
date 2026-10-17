#!/bin/sh
set -eu
BASE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
STATE=/root/cherrypi-fastboot-stage1/state
[ "$(id -u)" -eq 0 ] || { echo 'Root required';exit 1; }
pidof gpsp cherrypi-launcher >/dev/null 2>&1 && { echo 'Recover serial login before installing';exit 1; }
grep -qs ' /mnt/boot vfat rw' /proc/mounts || { echo 'Mount SD p1 read-write on /mnt/boot first';exit 1; }
grep -qs '^/dev/mmcblk0p1 /mnt/boot ' /proc/mounts || { echo 'Expected SD p1 at /mnt/boot';exit 1; }
[ -f /mnt/games/DISABLE_GBA_AUTOSTART ] || { echo 'Keep DISABLE_GBA_AUTOSTART during installation';exit 1; }
(cd "$BASE" && sha256sum -c SHA256SUMS)
check() {
    current=$(sha256sum "$1" | cut -d ' ' -f 1)
    grep -qx "$current" "$BASE/$2" || { echo "Unsupported baseline: $1 SHA256=$current";exit 1; }
}
check /mnt/boot/boot.scr BOOT_BASELINES
check /mnt/boot/suniv-f1c100s-generic.dtb DTB_BASELINES
check /usr/local/bin/cherrypi-launcher MENU_BASELINES
check /root/gpsp/gpsp GPSP_BASELINES
mkdir -p "$STATE"
if [ ! -f "$STATE/backup-complete" ]; then
    cp -p /mnt/boot/boot.scr "$STATE/boot.scr"
    cp -p /mnt/boot/suniv-f1c100s-generic.dtb "$STATE/suniv-f1c100s-generic.dtb"
    cp -p /usr/local/bin/cherrypi-launcher "$STATE/cherrypi-launcher"
    (cd "$STATE" && sha256sum boot.scr suniv-f1c100s-generic.dtb cherrypi-launcher > SHA256SUMS)
    touch "$STATE/backup-complete"
fi
cp "$BASE/boot.scr" /mnt/boot/boot.scr.fast-new
cp "$BASE/suniv-f1c100s-generic.dtb" /mnt/boot/suniv-f1c100s-generic.dtb.fast-new
cp "$BASE/cherrypi-launcher" /usr/local/bin/cherrypi-launcher.fast-new
chmod 755 /usr/local/bin/cherrypi-launcher.fast-new
mv /mnt/boot/boot.scr.fast-new /mnt/boot/boot.scr
mv /mnt/boot/suniv-f1c100s-generic.dtb.fast-new /mnt/boot/suniv-f1c100s-generic.dtb
mv /usr/local/bin/cherrypi-launcher.fast-new /usr/local/bin/cherrypi-launcher
sync
echo 'Installed experimental fastboot stage1: quieter kernel, two devices disabled, on-demand ROM scan.'
echo 'Kernel zImage, U-Boot and optimized gpSP are unchanged. Remove recovery marker and reboot to measure.'
