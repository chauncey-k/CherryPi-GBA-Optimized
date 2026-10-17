#!/bin/sh
set -eu
BASE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
STATE=/root/cherrypi-cover-preview-20261017/state
MENU=/usr/local/bin/cherrypi-launcher
[ "$(id -u)" -eq 0 ] || { echo 'Root required';exit 1; }
pidof gpsp cherrypi-launcher >/dev/null 2>&1 && { echo 'Exit game and pause autostart with Ctrl+C first';exit 1; }
[ ! -f /run/cherrypi-autostart.ready ] || { echo 'Pause autostart first';exit 1; }
[ -f /mnt/games/DISABLE_GBA_AUTOSTART ] || { echo 'Keep the DISABLE_GBA_AUTOSTART file during installation';exit 1; }
[ -f /root/gpsp/lib/libpng16.so.16 ] && [ -f /root/gpsp/lib/libz.so.1 ] || { echo 'Existing gpSP PNG/zlib runtime libraries required';exit 1; }
(cd "$BASE" && sha256sum -c SHA256SUMS)
current=$(sha256sum "$MENU" | cut -d ' ' -f 1)
grep -qx "$current" "$BASE/MENU_BASELINES" || { echo "Unsupported menu baseline: $current";exit 1; }
mkdir -p "$STATE"
if [ ! -f "$STATE/backup-complete" ]; then
    cp -p "$MENU" "$STATE/cherrypi-launcher"
    (cd "$STATE" && sha256sum cherrypi-launcher > SHA256SUMS)
    touch "$STATE/backup-complete"
fi
(cd "$STATE" && sha256sum -c SHA256SUMS)
cp "$BASE/cherrypi-launcher" "$MENU.cover-new"
chmod 755 "$MENU.cover-new"
mv "$MENU.cover-new" "$MENU"
mkdir -p /mnt/games/covers
sync
echo 'Cover preview installed. Put same-name PNG covers in GAMES/covers; remove maintenance marker and reboot.'
