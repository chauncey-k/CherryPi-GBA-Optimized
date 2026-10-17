#!/usr/bin/env bash
set -euo pipefail
BASE=$(cd "$(dirname "$0")" && pwd)
HOST=${CHERRYPI_SDK_HOST:?Set CHERRYPI_SDK_HOST}
MKIMAGE=${CHERRYPI_MKIMAGE:?Set CHERRYPI_MKIMAGE}
mkdir -p "$BASE/build" "$BASE/cherrypi-fastboot"
CC="$HOST/bin/arm-linux-gcc"
SYSROOT=$("$CC" --print-sysroot)
export PKG_CONFIG_SYSROOT_DIR="$SYSROOT" PKG_CONFIG_LIBDIR="$SYSROOT/usr/lib/pkgconfig"
"$CC" -O2 -Wall -Wextra -march=armv5te -mtune=arm926ej-s -mfloat-abi=soft \
  $("$HOST/bin/pkg-config" --cflags sdl) "$BASE"/source/{launcher,library,platform,settings}.c \
  $("$HOST/bin/pkg-config" --libs sdl) -Wl,-rpath,/root/gpsp/lib -o "$BASE/build/cherrypi-launcher"
cp "$BASE/build/cherrypi-launcher" "$BASE/cherrypi-fastboot/cherrypi-launcher"
"$HOST/bin/arm-linux-strip" "$BASE/cherrypi-fastboot/cherrypi-launcher"
cp "$BASE/baseline.dtb" "$BASE/cherrypi-fastboot/suniv-f1c100s-generic.dtb"
fdtput -t s "$BASE/cherrypi-fastboot/suniv-f1c100s-generic.dtb" /soc/spi@1c05000 status disabled
fdtput -t s "$BASE/cherrypi-fastboot/suniv-f1c100s-generic.dtb" /soc/video-codec@1c0e000 status disabled
SOURCE_DATE_EPOCH=1792195200 "$MKIMAGE" \
  -A arm -O linux -T script -C none -n 'Cherry Pi fastboot stage1' -d "$BASE/boot.cmd" "$BASE/cherrypi-fastboot/boot.scr"
gcc -std=c99 -Wall -Wextra -Werror -Wno-format-truncation -Wno-misleading-indentation \
  -I"$BASE/tests" -I"$BASE/source" -DCONFIG_FILE='"config"' -DBACKLIGHT_DIR='"backlight"' \
  -DHISTORY_FILE='"history"' -DRUN_DIR='"run"' -DEMULATOR_WRAPPER='"./mock-emulator.sh"' \
  "$BASE/tests/test_menu.c" "$BASE/source/library.c" -Wl,--wrap=library_scan -o "$BASE/build/test-menu"
(cd "$BASE/build" && ./test-menu)
