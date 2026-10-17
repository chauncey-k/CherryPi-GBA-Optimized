#!/bin/sh
# Run without the profiling timer; bounded diagnostics remain in tmpfs.
set -eu
pidof gpsp cherrypi-launcher >/dev/null 2>&1 && { echo 'Exit game/menu before testing';exit 1; }
unset CP_PROFILE
VOLUME=25
[ ! -r /etc/cherrypi-autostart.conf ] || . /etc/cherrypi-autostart.conf
case "$VOLUME" in ''|*[!0-9]*) VOLUME=25;; esac
[ "$VOLUME" -le 100 ] || VOLUME=25
/root/audio-tools/amixer -c 0 sset 'Headphone Source' 'DAC'
/root/audio-tools/amixer -c 0 sset 'DAC' 100%
/root/audio-tools/amixer -c 0 sset 'Headphone' "$VOLUME%" unmute
/root/audio-tools/audio-mode dsp
LOG=/tmp/cherrypi-memory-test.txt
monitor() {
    n=0
    while [ "$n" -lt 300 ]; do
        printf '\nSAMPLE %s uptime=' "$n"
        cat /proc/uptime
        grep -E '^(MemTotal|MemFree|MemAvailable|CmaTotal|CmaFree|SReclaimable|SwapFree):' /proc/meminfo
        for p in $(pidof gpsp || :); do
            echo "PID $p"
            grep -E '^(VmRSS|VmData|VmSwap|RssAnon|RssFile|RssShmem):' "/proc/$p/status" 2>/dev/null || :
        done
        n=$((n+1));sleep 2
    done
}
: > "$LOG"
monitor >> "$LOG" 2>&1 &
monitor_pid=$!
cleanup() { kill "$monitor_pid" 2>/dev/null || :;wait "$monitor_pid" 2>/dev/null || :; }
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
result=0
SDL_VIDEODRIVER=dummy LD_LIBRARY_PATH=/root/gpsp/lib /usr/local/bin/cherrypi-launcher || result=$?
cleanup
trap - EXIT
printf '\nFINAL STATUS %s\n' "$result" >> "$LOG"
grep -E '^(MemTotal|MemFree|MemAvailable|CmaTotal|CmaFree):' /proc/meminfo >> "$LOG"
dmesg | tail -n 80 >> "$LOG"
echo "Memory test ended. Read: cat $LOG"
exit "$result"
