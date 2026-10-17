#!/usr/bin/env bash
set -euo pipefail
BASE=$(cd "$(dirname "$0")" && pwd)
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
tar -xzf "$BASE/cherrypi-display-session-20261017.tar.gz" -C "$TMP"
(cd "$TMP/cherrypi-display-session" && sha256sum -c SHA256SUMS)
bash "$TMP/cherrypi-display-session/tests/run.sh"
echo 'PASS final extracted archive checksums, state machine, supervisor and installer tests'
