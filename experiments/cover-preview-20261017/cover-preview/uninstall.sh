#!/bin/sh
set -eu
STATE=/root/cherrypi-cover-preview-20261017/state
MENU=/usr/local/bin/cherrypi-launcher
[ "$(id -u)" -eq 0 ] || exit 1
pidof gpsp cherrypi-launcher >/dev/null 2>&1 && { echo 'Exit game and pause autostart first';exit 1; }
[ ! -f /run/cherrypi-autostart.ready ] || { echo 'Pause autostart first';exit 1; }
[ -f "$STATE/backup-complete" ] || { echo 'Backup missing';exit 1; }
(cd "$STATE" && sha256sum -c SHA256SUMS)
cp -p "$STATE/cherrypi-launcher" "$MENU.rollback"
mv "$MENU.rollback" "$MENU"
sync
echo 'Previous menu restored; cover images retained.'
