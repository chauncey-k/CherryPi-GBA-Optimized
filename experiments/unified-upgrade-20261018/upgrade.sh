#!/bin/sh
# Transactional upgrade for an existing Cherry Pi independent-menu system.
# GPL-2.0-or-later. No ROMs, BIOS, saves or user configuration are installed.
set -eu
BASE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
STORE=/root/cherrypi-unified-20261018
FIRST=$STORE/first-backup
PENDING=$STORE/pending
LOCK=/run/cherrypi-upgrade.lock
locked=0
applying=0
targets() {
    cat <<'TARGETS'
gpsp /root/gpsp/gpsp 755
menu /usr/local/bin/cherrypi-launcher 755
supervisor /usr/local/sbin/cherrypi-autostart 755
display /usr/local/sbin/cherrypi-display-session 755
control /usr/local/sbin/cherrypi-device-control 755
boot /mnt/boot/boot.scr 644
dtb /mnt/boot/suniv-f1c100s-generic.dtb 644
release /etc/cherrypi-release 644
TARGETS
}
die() { echo "ERROR: $*" >&2;exit 1; }
digest() { sha256sum "$1" | cut -d ' ' -f 1; }
safe_temporary() {
    [ ! -L "$1" ] || die "Symlink temporary file refused: $1"
    [ ! -e "$1" ] || [ -f "$1" ] || die "Temporary path is not a file: $1"
}
check_backup() {
    [ -f "$1/READY" ] && [ ! -L "$1" ] || die "Incomplete backup: $1"
    (cd "$1" && sha256sum -c SHA256SUMS) || die "Backup checksum failed: $1"
    targets > "$STORE/expected-targets"
    cmp "$STORE/expected-targets" "$1/TARGETS" >/dev/null || die 'Backup target list differs'
    while read -r key path mode; do
        if [ -f "$1/$key" ] && [ ! -L "$1/$key" ]; then
            [ ! -e "$1/$key.absent" ] || die "Ambiguous backup: $key"
        else
            [ -f "$1/$key.absent" ] && [ ! -L "$1/$key.absent" ] || die "Missing backup: $key"
        fi
    done <<EOF
$(targets)
EOF
}
snapshot() {
    [ ! -e "$1" ] || die "Snapshot already exists: $1"
    mkdir "$1"
    targets > "$1/TARGETS"
    while read -r key path mode; do
        [ ! -L "$path" ] || die "Symlink target refused: $path"
        if [ -f "$path" ]; then cp -p "$path" "$1/$key";
        elif [ ! -e "$path" ]; then : > "$1/$key.absent";
        else die "Target is not a regular file: $path";fi
    done <<EOF
$(targets)
EOF
    (cd "$1" && sha256sum TARGETS gpsp menu supervisor control boot dtb > SHA256SUMS
     for key in display release; do
        if [ -f "$key" ]; then sha256sum "$key";else sha256sum "$key.absent";fi
     done >> SHA256SUMS)
    touch "$1/READY"
    sync
    check_backup "$1"
}
restore() {
    check_backup "$1" || return 1
    while read -r key path mode; do
        if [ -f "$1/$key" ]; then
            safe_temporary "$path.unified-restore"
            cp -p "$1/$key" "$path.unified-restore" && mv "$path.unified-restore" "$path" || return 1
            [ "$(digest "$path")" = "$(digest "$1/$key")" ] || return 1
        else
            rm -f "$path" || return 1
            [ ! -e "$path" ] && [ ! -L "$path" ] || return 1
        fi
    done <<EOF
$(targets)
EOF
    sync
}
cleanup() {
    result=$?
    trap - EXIT INT TERM
    if [ "$applying" -eq 1 ]; then
        echo 'Upgrade interrupted; restoring this transaction backup.' >&2
        if restore "$PENDING"; then
            rm -f "$PENDING/APPLYING";sync
            echo 'Previous files restored; maintenance marker retained.' >&2
        else
            echo 'Restore incomplete. Keep maintenance mode; run upgrade.sh recover.' >&2
        fi
        result=1
    fi
    [ "$locked" -eq 0 ] || rmdir "$LOCK" 2>/dev/null || :
    exit "$result"
}
quiescent() {
    [ "$(id -u)" -eq 0 ] || die 'Root required'
    pidof gpsp cherrypi-launcher >/dev/null 2>&1 && die 'Exit game/menu and pause autostart with Ctrl+C first'
    [ ! -f /run/cherrypi-autostart.ready ] || die 'Pause autostart first'
    [ -f /mnt/games/DISABLE_GBA_AUTOSTART ] && [ ! -L /mnt/games/DISABLE_GBA_AUTOSTART ] || die 'Create the regular maintenance marker on GAMES'
    grep -qs '^/dev/mmcblk0p1 /mnt/boot vfat rw[, ]' /proc/mounts || die 'Mount SD p1 read-write at /mnt/boot first'
    [ ! -L "$STORE" ] && [ ! -L "$PENDING" ] && [ ! -L "$FIRST" ] || die 'Symlink backup directory refused'
    mkdir -p "$STORE"
    mkdir "$LOCK" 2>/dev/null || die 'Upgrade lock exists; confirm no other upgrade is running before removing it'
    locked=1
    trap cleanup EXIT
    trap 'exit 130' INT
    trap 'exit 143' TERM
}
package_check() {
    (cd "$BASE" && sha256sum -c SHA256SUMS) || die 'Package checksum failed'
    sh -n "$BASE/payload/supervisor"
    sh -n "$BASE/payload/control"
}
baseline_check() {
    grep -qxF 'ttyS0::respawn:/usr/local/sbin/cherrypi-autostart' /etc/inittab || die 'Existing independent autostart required'
    [ -r /etc/cherrypi-autostart.conf ] || die 'Existing settings required'
    for file in /root/gpsp/run-gpsp.sh /root/audio-tools/amixer /root/audio-tools/audio-mode; do
        [ -x "$file" ] || die "Missing runtime: $file"
    done
    for file in libSDL-1.2.so.0 libpng16.so.16 libz.so.1; do
        [ -f "/root/gpsp/lib/$file" ] || die "Missing runtime library: $file"
    done
    [ -f /root/gpsp/gba_bios.bin ] && [ "$(wc -c < /root/gpsp/gba_bios.bin)" -eq 16384 ] || die 'Existing 16384-byte BIOS required'
    while read -r key path mode; do
        [ ! -L "$path" ] || die "Symlink target refused: $path"
        safe_temporary "$path.unified-new"
        safe_temporary "$path.unified-restore"
        if [ -f "$path" ]; then hash=$(digest "$path");
        elif [ ! -e "$path" ]; then hash=absent;
        else die "Not a regular file: $path";fi
        grep -qx "$hash" "$BASE/baselines/$key" || die "Unsupported $key SHA256=$hash"
    done <<EOF
$(targets)
EOF
    available=$(df -k /root | awk 'END {print $4}')
    [ "$available" -ge 8192 ] || die 'At least 8 MiB free on root filesystem required'
    available=$(df -k /mnt/boot | awk 'END {print $4}')
    [ "$available" -ge 128 ] || die 'At least 128 KiB free on BOOT required'
}
prepare() {
    if [ -d "$PENDING" ]; then
        [ ! -e "$PENDING/APPLYING" ] || die 'Interrupted transaction; run upgrade.sh recover first'
        # Only our fixed scratch directory, never a user-selected path.
        rm -rf "$PENDING"
    fi
    snapshot "$PENDING"
    if [ ! -d "$FIRST" ]; then
        [ ! -e "$STORE/first-new" ] || die 'Incomplete first backup; inspect first-new before retrying'
        cp -pR "$PENDING" "$STORE/first-new"
        check_backup "$STORE/first-new"
        mv "$STORE/first-new" "$FIRST"
        sync
    else check_backup "$FIRST";fi
    touch "$PENDING/APPLYING";sync
    applying=1
}
finish() {
    sync
    rm -f "$PENDING/APPLYING";sync
    applying=0
    echo 'SUCCESS. Maintenance marker retained. Remove it and reboot when ready.'
}
case "${1:-status}" in
 status)
    [ ! -f /etc/cherrypi-release ] || cat /etc/cherrypi-release
    while read -r key path mode; do
        if [ -f "$path" ]; then printf '%s %s\n' "$key" "$(digest "$path")";else echo "$key absent";fi
    done <<EOF
$(targets)
EOF
    [ ! -e "$PENDING/APPLYING" ] || echo 'RECOVERY REQUIRED: interrupted transaction'
    ;;
 install)
    quiescent;package_check;baseline_check;prepare
    while read -r key path mode; do
        cp "$BASE/payload/$key" "$path.unified-new"
        chmod "$mode" "$path.unified-new"
        mv "$path.unified-new" "$path"
        [ "$(digest "$path")" = "$(digest "$BASE/payload/$key")" ] || die "Write verification failed: $key"
    done <<EOF
$(targets)
EOF
    finish
    ;;
 rollback)
    quiescent;package_check
    [ ! -e "$PENDING/APPLYING" ] || die 'Run recover first'
    check_backup "$FIRST"
    # Do not overwrite an unrelated later upgrade.
    while read -r key path mode; do
        [ ! -L "$path" ] || die "Symlink target refused: $path"
        if [ -f "$path" ] && [ "$(digest "$path")" = "$(digest "$BASE/payload/$key")" ]; then :;
        elif [ -f "$FIRST/$key" ] && [ -f "$path" ] && [ "$(digest "$path")" = "$(digest "$FIRST/$key")" ]; then :;
        elif [ -f "$FIRST/$key.absent" ] && [ ! -e "$path" ]; then :;
        else die "Changed $key; refusing to overwrite it";fi
    done <<EOF
$(targets)
EOF
    prepare
    restore "$FIRST"
    finish
    ;;
 recover)
    quiescent
    [ -f "$PENDING/APPLYING" ] || die 'No interrupted transaction'
    restore "$PENDING"
    rm -f "$PENDING/APPLYING";sync
    echo 'Transaction restored. Maintenance marker retained.'
    ;;
 *) die 'Usage: sh upgrade.sh status|install|rollback|recover';;
esac
