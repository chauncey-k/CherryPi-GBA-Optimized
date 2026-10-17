#!/usr/bin/env bash
set -euo pipefail
BASE=$(cd "$(dirname "$0")/.." && pwd)
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
gcc -std=gnu99 -Wall -Wextra -Werror -g -fsanitize=address,undefined \
  "$BASE/tests/test_display.c" \
  -Wl,--wrap=open,--wrap=close,--wrap=ioctl,--wrap=tcflush,--wrap=write,--wrap=rename,--wrap=geteuid \
  -o "$TMP/test-display"
(cd "$TMP" && ./test-display)
python3 "$BASE/tests/test_supervisor.py"
python3 "$BASE/tests/test_install.py"
PAYLOAD="$BASE/display-session"
[ -d "$PAYLOAD" ] || PAYLOAD="$BASE"
for f in "$PAYLOAD"/*.sh; do busybox sh -n "$f"; done
