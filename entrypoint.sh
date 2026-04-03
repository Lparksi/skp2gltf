#!/bin/sh
set -eu

# Detect architecture
ARCH=$(uname -m)
echo "Detected architecture: $ARCH"

# For arm64/aarch64, we need to use Box64 to emulate x86_64 with high performance
if [ "$ARCH" = "aarch64" ] || [ "$ARCH" = "arm64" ]; then
    echo "Running on arm64 architecture, using Box64 for high-performance emulation"
    if command -v box64 >/dev/null 2>&1; then
        EMULATOR_BIN="box64"
    else
        echo "box64 is not installed in this image" >&2
        exit 127
    fi
fi

if { [ "$ARCH" = "aarch64" ] || [ "$ARCH" = "arm64" ]; } && [ -x /usr/lib/wine/wine64 ]; then
    WINE_BIN="/usr/lib/wine/wine64"
elif command -v wine64 >/dev/null 2>&1; then
    WINE_BIN="$(command -v wine64)"
elif [ -x /usr/lib/wine/wine64 ]; then
    WINE_BIN="/usr/lib/wine/wine64"
elif command -v wine >/dev/null 2>&1; then
    WINE_BIN="$(command -v wine)"
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

# Service mode: start the FastAPI server
if [ "${1:-}" = "--service" ]; then
    echo "Starting skp2gltf API service on port 8000..."
    # Ensure uv is used if available
    if command -v uv >/dev/null 2>&1; then
        exec uv run uvicorn api:app --host 0.0.0.0 --port 8000
    else
        exec python3 -m uvicorn api:app --host 0.0.0.0 --port 8000
    fi
fi

set +e
# Use Box64 emulation for arm64 architecture
if [ "$ARCH" = "aarch64" ] || [ "$ARCH" = "arm64" ]; then
    "$EMULATOR_BIN" "$WINE_BIN" /app/skp2gltf.exe "$@"
else
    "$WINE_BIN" /app/skp2gltf.exe "$@"
fi
EXIT_CODE="$?"
set -e

exit "$EXIT_CODE"
