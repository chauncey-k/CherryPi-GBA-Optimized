#!/usr/bin/env bash
set -euo pipefail
BASE=$(cd "$(dirname "$0")" && pwd)
HOST=${CHERRYPI_SDK_HOST:?Set CHERRYPI_SDK_HOST to a Buildroot host directory with ARMv5 musl/SDL/libpng/zlib}
test -x "$HOST/bin/arm-linux-gcc"
mkdir -p "$BASE/build"
make -C "$BASE/build" -f "$BASE/source/gpsp/cherrypi/Makefile" HOST="$HOST" SOURCE="$BASE/source/gpsp" -j"${JOBS:-2}"

cp "$BASE/build/gpsp" "$BASE/build/gpsp.runtime"
"$HOST/bin/arm-linux-nm" -n -S --defined-only "$BASE/build/gpsp" > "$BASE/build/gpsp.nm"
"$HOST/bin/arm-linux-strip" "$BASE/build/gpsp.runtime"
if [ "${1:-}" = --stage ]; then
    cp "$BASE/build/gpsp.runtime" "$BASE/cherrypi-profile/gpsp"
    cp "$BASE/build/gpsp.nm" "$BASE/cherrypi-profile/gpsp.nm"
fi
echo 'Built candidate in build/. --stage explicitly updates runtime files; then regenerate package and manifest.'
