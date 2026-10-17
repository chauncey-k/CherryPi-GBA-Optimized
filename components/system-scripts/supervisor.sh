#!/bin/sh
# BusyBox init attaches ttyS0; gpSP reads GPIO keys and can run without a PC.
set -u
PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
export PATH
CONFIG=/etc/cherrypi-autostart.conf
PAUSED=/run/cherrypi-autostart.paused
LOG=/tmp/cherrypi-autostart.log
GAMES=/mnt/games
ROMDIR=$GAMES/roms
VOLUME=65
[ ! -r "$CONFIG" ] || . "$CONFIG"

fallback() {
    rm -f /run/cherrypi-autostart.ready /run/cherrypi-autostart.pid
    echo "[GBA] $*; restoring serial login."
    echo "[GBA] Details: $LOG"
    stty sane 2>/dev/null || :
    exec /sbin/getty -L ttyS0 115200 vt100
}
log() { printf '%s\n' "$*" >> "$LOG"; }
trap 'touch "$PAUSED"' INT
trap 'rm -f /run/cherrypi-autostart.ready /run/cherrypi-autostart.pid; sync; exit 0' TERM
mkdir -p /run "$GAMES" || fallback 'Cannot prepare directories'
[ -t 0 ] || fallback 'No terminal on standard input'
[ ! -f "$PAUSED" ] || fallback 'Paused for this boot'
[ ! -f /root/.disable-gba-autostart ] || fallback 'Disabled in root filesystem'
case "$VOLUME" in ''|*[!0-9]*) fallback 'Invalid volume';; esac
[ "$VOLUME" -le 100 ] || fallback 'Volume must be 0..100'
if ! grep -qs " $GAMES " /proc/mounts; then
    tries=0
    while [ ! -b /dev/mmcblk0p3 ] && [ "$tries" -lt 10 ]; do
        sleep 1
        tries=$((tries + 1))
    done
    mount -t vfat /dev/mmcblk0p3 "$GAMES" || echo 'GAMES mount failed; menu will show no games'
fi
[ ! -f "$GAMES/DISABLE_GBA_AUTOSTART" ] || fallback 'Recovery marker on GAMES'
grep -q 'Name="cherrypi-gamepad"' /proc/bus/input/devices || fallback 'GPIO gamepad not detected'
[ -x /root/gpsp/run-gpsp.sh ] && [ -x /root/gpsp/gpsp ] || fallback 'gpSP is missing'
[ -f /root/gpsp/gba_bios.bin ] || fallback 'BIOS is missing'
[ "$(wc -c < /root/gpsp/gba_bios.bin)" -eq 16384 ] || fallback 'BIOS size is invalid'
[ -d "$ROMDIR" ] || ROMDIR=$GAMES
[ -x /usr/local/bin/cherrypi-launcher ] || fallback 'Main menu is missing'
printf '%s\n' "$ROMDIR" > /root/gpsp/romdir.txt || fallback 'Cannot set ROM directory'
: > "$LOG"
for tool in amixer audio-mode; do
    [ -x "/root/audio-tools/$tool" ] || fallback "Missing audio tool $tool"
done
/root/audio-tools/amixer -c 0 sset 'Headphone Source' 'DAC' >> "$LOG" 2>&1 || fallback 'Audio route failed'
/root/audio-tools/amixer -c 0 sset 'DAC' 100% >> "$LOG" 2>&1 || fallback 'DAC setting failed'
/root/audio-tools/amixer -c 0 sset 'Headphone' "$VOLUME%" unmute >> "$LOG" 2>&1 || fallback 'Volume setting failed'
if ! grep -q '^export SDL_AUDIODRIVER=dsp$' /root/gpsp/run-gpsp.sh; then
    /root/audio-tools/audio-mode dsp >> "$LOG" 2>&1 || fallback 'Audio backend setting failed'
fi
# BRIGHTNESS is optional: old configurations keep the kernel default.
if [ -n "${BRIGHTNESS:-}" ]; then
    /usr/local/sbin/cherrypi-device-control brightness "$BRIGHTNESS" >> "$LOG" 2>&1 || log 'Saved brightness unavailable; retaining current backlight'
fi
failures=0
echo $$ > /run/cherrypi-autostart.pid
touch /run/cherrypi-autostart.ready
while :; do
    [ ! -f "$PAUSED" ] || fallback 'Interrupted; paused for this boot'
    [ ! -f /root/.disable-gba-autostart ] || fallback 'Disabled'
    [ ! -f "$GAMES/DISABLE_GBA_AUTOSTART" ] || fallback 'Recovery marker on GAMES'
    cp "$LOG" "$LOG.previous" 2>/dev/null || :
    : > "$LOG"
    started=$(date +%s)
    log "Opening main menu; ROM directory=$ROMDIR volume=$VOLUME"
    rm -f /run/cherrypi-launcher-game
    SDL_VIDEODRIVER=dummy LD_LIBRARY_PATH=/root/gpsp/lib /usr/local/bin/cherrypi-launcher >> "$LOG" 2>&1
    result=$?
    elapsed=$(( $(date +%s) - started ))
    log "Menu/game exited: status=$result duration=$elapsed seconds"
    sync
    if [ -f /run/cherrypi-poweroff-request ]; then
        [ "$result" -eq 0 ] || { rm -f /run/cherrypi-poweroff-request; fallback 'Shutdown cancelled because gpSP failed'; }
        log 'gpSP saved and exited; shutting down'
        rm -f /run/cherrypi-autostart.ready
        sync
        /sbin/poweroff >> "$LOG" 2>&1 || { rm -f /run/cherrypi-poweroff-request; fallback 'Poweroff command failed'; }
        # Stay alive until init terminates us; do not respawn the game list.
        while :; do sleep 60; done
    fi
    if [ -f /run/cherrypi-launcher-game ]; then
        # The launcher replaced itself with gpSP. Always return home after a game.
        if [ "$result" -ne 0 ]; then
            printf 'Game exited with status %s; see serial log\n' "$result" > /run/cherrypi-launcher-error
        fi
        failures=0
    elif [ "$result" -ne 0 ] || [ "$elapsed" -lt 3 ]; then
        failures=$((failures + 1))
    else
        failures=0
    fi
    [ "$failures" -lt 3 ] || fallback 'Repeated short exits or failures'
    sleep 1
 done
