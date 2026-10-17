#!/bin/sh
set -eu
BASE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
STATE=/root/cherrypi-rom-mmap-20261017/state
[ "$(id -u)" -eq 0 ] || exit 1
pidof gpsp cherrypi-launcher >/dev/null 2>&1 && { echo 'Recover serial login before installing'; exit 1; }
[ -x /root/gpsp/gpsp ] && [ -x /usr/local/bin/cherrypi-launcher ] || { echo 'Existing independent main menu required'; exit 1; }
(cd "$BASE" && sha256sum -c SHA256SUMS)
current=$(sha256sum /root/gpsp/gpsp | cut -d ' ' -f 1)
new=$(sha256sum "$BASE/gpsp" | cut -d ' ' -f 1)
expected_file="$BASE/BASELINE_SHA256"
if ! grep -qx "$current" "$expected_file" && [ "$current" != "$new" ]; then
    echo 'Expected verified smc-live, 24MiB candidate, or this exact ROM-mmap candidate.'
    echo "Current gpSP SHA256: $current"
    exit 1
fi
mkdir -p "$STATE"
if [ ! -f "$STATE/gpsp" ]; then cp -p /root/gpsp/gpsp "$STATE/gpsp"; fi
cp "$BASE/gpsp" /root/gpsp/gpsp.quick-new
chmod 755 /root/gpsp/gpsp.quick-new
mv /root/gpsp/gpsp.quick-new /root/gpsp/gpsp
sync
echo 'Installed private ROM file-mapping candidate with existing audio instruction optimization. Existing states and device configuration preserved.'
echo 'Keep serial recovery enabled; run run-test.sh to test state overwrite without sampling.'
