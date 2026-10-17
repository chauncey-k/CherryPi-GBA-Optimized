#!/usr/bin/env bash
set -euo pipefail
BASE=$(cd "$(dirname "$0")/.." && pwd)
HOST=${CHERRYPI_SDK_HOST:?Set CHERRYPI_SDK_HOST}
SYSROOT=$("$HOST/bin/arm-linux-gcc" --print-sysroot)
mkdir -p "$BASE/build/previews"
export PREVIEW_DIR="$BASE/build/previews"
WORK=$(mktemp -d "$BASE/build/test-run.XXXXXX")
cd "$WORK"
python3 "$BASE/tests/fixtures.py"
COMMON=(-std=c99 -Wall -Wextra -Werror -Wno-format-truncation -Wno-misleading-indentation -I"$BASE/tests" -I"$BASE/source" -I"$SYSROOT/usr/include/libpng16" -DCOVER_DIR='"covers"')
gcc "${COMMON[@]}" -g -fsanitize=address,undefined "$BASE/tests/test_cover.c" "$BASE/source/cover.c" -l:libpng16.so.16 -o test-cover
./test-cover
gcc "${COMMON[@]}" -g -fsanitize=address,undefined -DCONFIG_FILE='"config"' -DBACKLIGHT_DIR='"backlight"' \
 -DHISTORY_FILE='"history"' -DRUN_DIR='"run"' -DEMULATOR_WRAPPER='"./mock-emulator.sh"' \
 "$BASE/tests/test_menu.c" "$BASE/source/library.c" "$BASE/source/cover.c" -Wl,--wrap=library_scan -l:libpng16.so.16 -o test-menu
ASAN_OPTIONS=detect_leaks=0 ./test-menu
python3 "$BASE/tests/test_install.py"
