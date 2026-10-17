#!/usr/bin/env bash
set -euo pipefail
BASE=$(cd "$(dirname "$0")" && pwd)
HOST=${CHERRYPI_SDK_HOST:?Set CHERRYPI_SDK_HOST}
CC="$HOST/bin/arm-linux-gcc"
SYSROOT=$("$CC" --print-sysroot)
export PKG_CONFIG_SYSROOT_DIR="$SYSROOT" PKG_CONFIG_LIBDIR="$SYSROOT/usr/lib/pkgconfig"
mkdir -p "$BASE/build"
"$CC" -O2 -Wall -Wextra -march=armv5te -mtune=arm926ej-s -mfloat-abi=soft \
  $("$HOST/bin/pkg-config" --cflags sdl) "$BASE"/source/{launcher,library,platform,settings}.c \
  $("$HOST/bin/pkg-config" --libs sdl) -Wl,-rpath,/root/gpsp/lib -o "$BASE/build/cherrypi-launcher"
