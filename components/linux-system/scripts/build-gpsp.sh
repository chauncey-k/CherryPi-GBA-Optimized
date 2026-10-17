#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$root"
for dep in libSDL.so libpng16.so libz.so; do
    test -f "build/rootfs/target/usr/lib/$dep" || { echo "Enable SDL, libpng and zlib in Buildroot first" >&2; exit 1; }
done
mkdir -p build/gpsp build/gpsp-package/gpsp/lib build/gpsp-package/gpsp/roms
make -C build/gpsp -f ../../gpsp/cherrypi/Makefile -j"${JOBS:-2}"
cp build/gpsp/gpsp build/gpsp-package/gpsp/gpsp
build/rootfs/host/bin/arm-linux-strip build/gpsp-package/gpsp/gpsp
cp gpsp/game_config.txt gpsp/COPYING.DOC board/cherrypi/run-gpsp.sh build/gpsp-package/gpsp/
cp gpsp/borders/border1.png build/gpsp-package/gpsp/border.png
for lib in libSDL-1.2.so.0 libpng16.so.16 libz.so.1; do
    cp -L "build/rootfs/target/usr/lib/$lib" "build/gpsp-package/gpsp/lib/$lib"
done
chmod +x build/gpsp-package/gpsp/run-gpsp.sh
(cd build/gpsp-package/gpsp && find . -type f ! -name SHA256SUMS -print0 | sort -z | xargs -0 sha256sum > SHA256SUMS)
tar -czf build/images/gpsp-cherrypi-20261009.tar.gz -C build/gpsp-package gpsp
(cd build/images && sha256sum gpsp-cherrypi-20261009.tar.gz > gpsp-cherrypi-20261009.tar.gz.sha256)
