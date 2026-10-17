#!/bin/sh
set -eu
STATE=/root/cherrypi-display-session-20261017/state
[ "$(id -u)" -eq 0 ] || exit 1
pidof gpsp cherrypi-launcher >/dev/null 2>&1 && { echo 'Recover serial login first';exit 1; }
[ ! -f /run/cherrypi-autostart.ready ] || { echo 'Pause autostart first';exit 1; }
[ -f "$STATE/backup-complete" ] || { echo 'Backup missing';exit 1; }
(cd "$STATE" && sha256sum -c SHA256SUMS)
[ ! -x /usr/local/sbin/cherrypi-display-session ] || /usr/local/sbin/cherrypi-display-session release
cp -p "$STATE/supervisor.sh" /usr/local/sbin/cherrypi-autostart.rollback
mv /usr/local/sbin/cherrypi-autostart.rollback /usr/local/sbin/cherrypi-autostart
if [ -f "$STATE/no-previous-helper" ]; then
    rm -f /usr/local/sbin/cherrypi-display-session
else
    cp -p "$STATE/cherrypi-display-session" /usr/local/sbin/cherrypi-display-session.rollback
    mv /usr/local/sbin/cherrypi-display-session.rollback /usr/local/sbin/cherrypi-display-session
fi
sync
echo 'Previous supervisor restored; gpSP, launcher and saves retained.'
