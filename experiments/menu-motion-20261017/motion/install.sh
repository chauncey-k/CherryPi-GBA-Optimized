#!/bin/sh
set -eu
BASE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
STATE=/root/cherrypi-menu-motion-20261017/state
MENU=/usr/local/bin/cherrypi-launcher
[ "$(id -u)" -eq 0 ] || { echo 'Root required';exit 1; }
pidof gpsp cherrypi-launcher >/dev/null 2>&1 && { echo 'Exit game and pause autostart with Ctrl+C first';exit 1; }
[ ! -f /run/cherrypi-autostart.ready ] || { echo 'Pause autostart first';exit 1; }
[ -f /mnt/games/DISABLE_GBA_AUTOSTART ] || { echo 'Keep the DISABLE_GBA_AUTOSTART file during installation';exit 1; }
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
cp "$BASE/cherrypi-launcher" "$MENU.motion-new"
chmod 755 "$MENU.motion-new"
mv "$MENU.motion-new" "$MENU"
sync
echo 'Menu animation installed. Remove maintenance marker and reboot.'
