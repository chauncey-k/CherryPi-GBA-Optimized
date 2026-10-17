#!/bin/sh
set -eu
BASE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
STATE=/root/cherrypi-display-session-20261017/state
[ "$(id -u)" -eq 0 ] || exit 1
pidof gpsp cherrypi-launcher >/dev/null 2>&1 && { echo 'Exit game/menu and recover serial login first';exit 1; }
[ ! -f /run/cherrypi-autostart.ready ] || { echo 'Pause autostart before installing';exit 1; }
[ -f /mnt/games/DISABLE_GBA_AUTOSTART ] || { echo 'Create DISABLE_GBA_AUTOSTART as a regular file on GAMES';exit 1; }
grep -qxF 'ttyS0::respawn:/usr/local/sbin/cherrypi-autostart' /etc/inittab || { echo 'Existing independent autostart required';exit 1; }
[ -x /usr/local/bin/cherrypi-launcher ] && [ -x /root/gpsp/gpsp ] || exit 1
(cd "$BASE" && sha256sum -c SHA256SUMS)
sh -n "$BASE/supervisor.sh"
current=$(sha256sum /usr/local/sbin/cherrypi-autostart | cut -d ' ' -f 1)
grep -qx "$current" "$BASE/SUPERVISOR_BASELINES" || { echo "Unsupported supervisor SHA256=$current";exit 1; }
if [ -e /usr/local/sbin/cherrypi-display-session ]; then
    previous=$(sha256sum /usr/local/sbin/cherrypi-display-session | cut -d ' ' -f 1)
    expected=$(sha256sum "$BASE/cherrypi-display-session" | cut -d ' ' -f 1)
    [ "$previous" = "$expected" ] || { echo 'Existing LCD helper differs; preserved';exit 1; }
fi
mkdir -p "$STATE"
if [ ! -f "$STATE/backup-complete" ]; then
    cp -p /usr/local/sbin/cherrypi-autostart "$STATE/supervisor.sh"
    if [ -f /usr/local/sbin/cherrypi-display-session ]; then
        cp -p /usr/local/sbin/cherrypi-display-session "$STATE/cherrypi-display-session"
    else
        touch "$STATE/no-previous-helper"
    fi
    (cd "$STATE" && sha256sum supervisor.sh > SHA256SUMS)
    [ ! -f "$STATE/cherrypi-display-session" ] || (cd "$STATE" && sha256sum cherrypi-display-session >> SHA256SUMS)
    touch "$STATE/backup-complete"
fi
cp "$BASE/cherrypi-display-session" /usr/local/sbin/cherrypi-display-session.new
chmod 755 /usr/local/sbin/cherrypi-display-session.new
mv /usr/local/sbin/cherrypi-display-session.new /usr/local/sbin/cherrypi-display-session
cp "$BASE/supervisor.sh" /usr/local/sbin/cherrypi-autostart.new
chmod 755 /usr/local/sbin/cherrypi-autostart.new
mv /usr/local/sbin/cherrypi-autostart.new /usr/local/sbin/cherrypi-autostart
sync
echo 'Installed LCD graphics session fix; game returns directly to menu.'
echo 'gpSP, launcher, boot files, settings and saves preserved. Remove recovery marker and reboot.'
