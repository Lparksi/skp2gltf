#!/bin/sh
set -eu

if command -v wine64 >/dev/null 2>&1; then
    WINE_BIN="wine64"
elif command -v wine >/dev/null 2>&1; then
    WINE_BIN="wine"
else
    echo "wine is not installed in this image" >&2
    exit 127
fi

# Disable Wine Mono/Gecko GUI installers in headless environments.
# This prevents first-run hangs where only the wine prefix line is shown.
: "${WINEDLLOVERRIDES:=mscoree,mshtml=}"
export WINEDLLOVERRIDES

Xvfb "${DISPLAY:-:99}" -screen 0 1024x768x24 >/dev/null 2>&1 &
XVFB_PID="$!"

cleanup() {
    kill "$XVFB_PID" >/dev/null 2>&1 || true
}
trap cleanup EXIT INT TERM

sleep 1

set +e
"$WINE_BIN" /app/skp2gltf.exe "$@"
EXIT_CODE="$?"
set -e

exit "$EXIT_CODE"
