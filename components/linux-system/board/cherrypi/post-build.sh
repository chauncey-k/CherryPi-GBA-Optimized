#!/usr/bin/env bash
set -euo pipefail
lcd_source_dir="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$1/usr/bin"
"$HOST_DIR/bin/arm-linux-gcc" -O2 -Wall -Wextra -Werror -static -march=armv5te -mfloat-abi=soft \
    "$lcd_source_dir/lcd-test.c" -o "$1/usr/bin/lcd-test"
