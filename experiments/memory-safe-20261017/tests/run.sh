#!/usr/bin/env bash
set -euo pipefail
BASE=$(cd "$(dirname "$0")/.." && pwd)
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
gcc -std=gnu99 -Wall -Wextra -Werror -g -fsanitize=address,undefined \
  -include stdint.h -include stddef.h -I"$BASE/source/gpsp/cherrypi" \
  "$BASE/tests/test_io.c" "$BASE/source/gpsp/cherrypi/state_io.c" "$BASE/source/gpsp/sha1.c" \
  -Wl,--wrap=write,--wrap=fsync,--wrap=rename,--wrap=close -o "$TMP/test-io"
(cd "$TMP" && ./test-io && ! compgen -G '*.tmp.*')
python3 "$BASE/tests/test_gui.py"
python3 "$BASE/tests/test_adapter.py"
python3 "$BASE/tests/test_text.py"
gcc -std=gnu99 -Wall -Wextra -Werror -Wno-misleading-indentation -g -fsanitize=address,undefined \
  -I"$BASE/tests" -I"$BASE/source/gpsp/cherrypi" "$BASE/tests/test_quick.c" -o "$TMP/test-quick"
(cd "$TMP" && ./test-quick)
python3 "$BASE/tests/test_helpers.py"
python3 "$BASE/tests/test_native_controls.py"
gcc -std=gnu99 -Wall -Wextra -Werror -g -fsanitize=undefined \
  -pthread "$BASE/tests/test_profile.c" -o "$TMP/test-profile"
(cd "$TMP" && ./test-profile && ./test-profile timer)
python3 "$BASE/tests/test_profile.py"
python3 "$BASE/tests/test_dma_smc.py"
PACKAGE="$BASE/cherrypi-profile"
[ ! -f "$BASE/install.sh" ] || PACKAGE="$BASE"
for script in "$PACKAGE"/*.sh; do busybox sh -n "$script"; done
