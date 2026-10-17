#!/bin/sh
set -eu
STATE=/root/cherrypi-fastboot-stage1/state
[ "$(id -u)" -eq 0 ] || exit 1
pidof gpsp cherrypi-launcher >/dev/null 2>&1 && { echo 'Recover serial login before rollback';exit 1; }
grep -qs '^/dev/mmcblk0p1 /mnt/boot vfat rw' /proc/mounts || { echo 'Mount SD p1 read-write at /mnt/boot';exit 1; }
[ -f "$STATE/backup-complete" ] || { echo 'Backup missing';exit 1; }
(cd "$STATE" && sha256sum -c SHA256SUMS)
cp -p "$STATE/boot.scr" /mnt/boot/boot.scr.fast-old
cp -p "$STATE/suniv-f1c100s-generic.dtb" /mnt/boot/suniv-f1c100s-generic.dtb.fast-old
cp -p "$STATE/cherrypi-launcher" /usr/local/bin/cherrypi-launcher.fast-old
mv /mnt/boot/boot.scr.fast-old /mnt/boot/boot.scr
mv /mnt/boot/suniv-f1c100s-generic.dtb.fast-old /mnt/boot/suniv-f1c100s-generic.dtb
mv /usr/local/bin/cherrypi-launcher.fast-old /usr/local/bin/cherrypi-launcher
sync
echo 'Previous boot script, DTB and launcher restored; optimized gpSP retained.'
