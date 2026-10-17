#!/usr/bin/env bash
set -euo pipefail
BASE=$(cd "$(dirname "$0")/.." && pwd)
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
tar -xzf "$BASE/cherrypi-rom-mmap-20261017.tar.gz" -C "$TMP"
(cd "$TMP/cherrypi-profile" && sha256sum -c SHA256SUMS)
bash "$TMP/cherrypi-profile/tests/run.sh"
python3 "$TMP/cherrypi-profile/tests/test_quick_install.py"
echo 'PASS complete archive checksums, extracted tests and install/rollback'
