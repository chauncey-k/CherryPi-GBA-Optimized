#!/usr/bin/env bash
set -euo pipefail
BASE=$(cd "$(dirname "$0")" && pwd)
HOST=${CHERRYPI_SDK_HOST:?Set CHERRYPI_SDK_HOST}
mkdir -p "$BASE/build"
make -C "$BASE/build" -f "$BASE/source/gpsp/cherrypi/Makefile" HOST="$HOST" SOURCE="$BASE/source/gpsp" -j"${JOBS:-2}"
cp "$BASE/build/gpsp" "$BASE/cherrypi-profile/gpsp"
"$HOST/bin/arm-linux-nm" -n -S --defined-only "$BASE/build/gpsp" > "$BASE/cherrypi-profile/gpsp.nm"
"$HOST/bin/arm-linux-strip" "$BASE/cherrypi-profile/gpsp"
