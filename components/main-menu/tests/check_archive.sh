#!/usr/bin/env bash
set -euo pipefail
BASE=$(cd "$(dirname "$0")/.." && pwd)
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
tar -xzf "$BASE/cherrypi-main-menu-20261014.tar.gz" -C "$TMP"
(cd "$TMP/cherrypi-main-menu" && sha256sum -c SHA256SUMS)
bash "$TMP/cherrypi-main-menu/tests/run.sh"
bash "$TMP/cherrypi-main-menu/build.sh"
cmp "$TMP/cherrypi-main-menu/cherrypi-launcher" "$BASE/cherrypi-main-menu/cherrypi-launcher"
echo 'PASS: archive checksum, extracted tests, reproducible ARM build'
