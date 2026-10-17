#!/bin/sh
set -eu
[ "$(id -u)" -eq 0 ] || exit 1
pidof gpsp cherrypi-launcher >/dev/null 2>&1 && { echo 'Recover serial login before rollback'; exit 1; }
[ -f /root/cherrypi-optimized-20261017/state/gpsp ] || { echo 'Previous gpSP backup missing'; exit 1; }
cp -p /root/cherrypi-optimized-20261017/state/gpsp /root/gpsp/gpsp.quick-rollback
mv /root/gpsp/gpsp.quick-rollback /root/gpsp/gpsp
sync
echo 'Previous gpSP restored; states and configuration retained.'
