#!/usr/bin/env bash
set -euo pipefail
BASE=$(cd "$(dirname "$0")/.." && pwd)
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
gcc -std=c99 -Wall -Wextra -Werror -Wno-format-truncation -Wno-misleading-indentation \
  -I"$BASE/tests" -I"$BASE/source" -DCONFIG_FILE='"config"' -DBACKLIGHT_DIR='"backlight"' \
  -DHISTORY_FILE='"history"' -DRUN_DIR='"run"' -DEMULATOR_WRAPPER='"./mock-emulator.sh"' \
  "$BASE/tests/test_menu.c" "$BASE/source/library.c" -Wl,--wrap=library_scan -o "$TMP/test-menu"
(cd "$TMP" && ./test-menu)
python3 "$BASE/test_payload.py"
python3 "$BASE/test_install.py"
for f in "$BASE/cherrypi-fastboot"/*.sh; do busybox sh -n "$f"; done
