#!/usr/bin/env bash
set -euo pipefail
BASE=$(cd "$(dirname "$0")/.." && pwd)
mkdir -p "$BASE/build/previews"
export PREVIEW_DIR="$BASE/build/previews"
WORK=$(mktemp -d "$BASE/build/test-run.XXXXXX")
cd "$WORK"
gcc -std=c99 -Wall -Wextra -Werror -g -fsanitize=address,undefined "$BASE/tests/test_motion.c" -o test-motion
./test-motion
gcc -std=c99 -Wall -Wextra -Werror -Wno-format-truncation -Wno-misleading-indentation \
 -g -fsanitize=address,undefined -I"$BASE/tests" -I"$BASE/source" \
 -DCONFIG_FILE='"config"' -DBACKLIGHT_DIR='"backlight"' \
 -DHISTORY_FILE='"history"' -DRUN_DIR='"run"' -DEMULATOR_WRAPPER='"./mock-emulator.sh"' \
 "$BASE/tests/test_menu.c" "$BASE/source/library.c" -Wl,--wrap=library_scan -o test-menu
ASAN_OPTIONS=detect_leaks=0 ./test-menu
python3 "$BASE/tests/test_install.py"
