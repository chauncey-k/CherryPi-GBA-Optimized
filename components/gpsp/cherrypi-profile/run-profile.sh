#!/bin/sh
set -eu
BASE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
pidof gpsp cherrypi-launcher >/dev/null 2>&1 && { echo 'Exit the game/menu before profiling'; exit 1; }
new=$(sha256sum "$BASE/gpsp" | cut -d ' ' -f 1)
current=$(sha256sum /root/gpsp/gpsp | cut -d ' ' -f 1)
[ "$current" = "$new" ] || { echo 'Install this profiler package first'; exit 1; }
rm -f /tmp/cherrypi-profile.txt
printf 'Profiler binary SHA256: %s\n' "$current"
echo 'Choose the target game, play the slow scene for 60 seconds, then exit normally.'
result=0
CP_PROFILE=1 SDL_VIDEODRIVER=dummy LD_LIBRARY_PATH=/root/gpsp/lib \
  /usr/local/bin/cherrypi-launcher || result=$?
if [ -s /tmp/cherrypi-profile.txt ]; then
    printf 'BINARY_SHA256 %s\n' "$current" >> /tmp/cherrypi-profile.txt
    echo 'Ready: cat /tmp/cherrypi-profile.txt'
else
    echo 'No new profile: a game must run and exit normally (not power off or kill -9).'
fi
exit "$result"
