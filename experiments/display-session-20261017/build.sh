#!/usr/bin/env bash
set -euo pipefail
BASE=$(cd "$(dirname "$0")" && pwd)
HOST=${CHERRYPI_SDK_HOST:?Set CHERRYPI_SDK_HOST}
PAYLOAD="$BASE/display-session"
[ ! -f "$BASE/install.sh" ] || PAYLOAD="$BASE"
mkdir -p "$BASE/build" "$PAYLOAD"
"$HOST/bin/arm-linux-gcc" -O2 -Wall -Wextra -Werror -march=armv5te -mtune=arm926ej-s -mfloat-abi=soft \
  "$BASE/source/display-session.c" -o "$BASE/build/cherrypi-display-session"
cp "$BASE/build/cherrypi-display-session" "$PAYLOAD/cherrypi-display-session"
"$HOST/bin/arm-linux-strip" "$PAYLOAD/cherrypi-display-session"
cp "$BASE/source/supervisor.sh" "$PAYLOAD/supervisor.sh"
