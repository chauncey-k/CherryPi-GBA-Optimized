#!/bin/sh
set -eu
BASE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
if [ "${1:-}" != "--platform-test" ] && { [ ! -f "$BASE/gba_bios.bin" ] || [ "$(wc -c < "$BASE/gba_bios.bin")" -ne 16384 ]; }; then
    echo "Put a genuine 16384-byte gba_bios.bin next to run-gpsp.sh."
    exit 1
fi
if [ "$#" -gt 1 ]; then
    echo "Usage: $0 [game.gba]"; exit 1
fi
if [ "$#" -eq 1 ] && [ "$1" != "--platform-test" ]; then
    [ -f "$1" ] || { echo "ROM not found: $1"; exit 1; }
    ROM=$(readlink -f "$1")
fi
export LD_LIBRARY_PATH="$BASE/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export SDL_VIDEODRIVER=dummy
export SDL_AUDIODRIVER=dummy
export SDL_NOMOUSE=1
printf 'WASD=direction J=A K=B U=L I=R Enter=Start Space=Select M=menu Q=quit\n'
# Restoring the terminal in the parent also covers abnormal program termination.
TTY_STATE=$(stty -g)
trap 'stty "$TTY_STATE"; sync' EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
cd "$BASE"
if [ "$#" -eq 1 ]; then
    "$BASE/gpsp" "${ROM:---platform-test}"
else
    "$BASE/gpsp"
fi
