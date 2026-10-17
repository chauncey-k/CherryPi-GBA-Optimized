#!/usr/bin/env bash
set -euo pipefail
BASE=$(cd "$(dirname "$0")/.." && pwd)
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
gcc -std=c99 -Wall -Wextra -Werror -Wno-format-truncation -Wno-misleading-indentation \
    -I"$BASE/tests" -I"$BASE/source" \
    -DCONFIG_FILE='"config"' -DBACKLIGHT_DIR='"backlight"' \
    -DHISTORY_FILE='"history"' -DRUN_DIR='"run"' -DEMULATOR_WRAPPER='"./mock-emulator.sh"' \
    "$BASE/tests/test_menu.c" "$BASE/source/library.c" -o "$TMP/test-menu"
(cd "$TMP" && ./test-menu)
PACKAGE="$BASE/cherrypi-main-menu"
[ ! -f "$BASE/install.sh" ] || PACKAGE="$BASE"
for f in "$PACKAGE"/*.sh; do bash -n "$f"; busybox sh -n "$f"; done
python3 "$BASE/tests/test_supervisor.py"
python3 "$BASE/tests/test_install.py"
python3 "$BASE/tests/test_native_controls.py"
