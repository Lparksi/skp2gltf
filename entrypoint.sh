#!/bin/sh
set -eu

# Detect architecture
ARCH=$(uname -m)
echo "Detected architecture: $ARCH"

# For arm64/aarch64, we need to use qemu to emulate x86_64
if [ "$ARCH" = "aarch64" ] || [ "$ARCH" = "arm64" ]; then
    echo "Running on arm64 architecture, using qemu-x86_64 for emulation"
    # Check if qemu-x86_64 is available
    if command -v qemu-x86_64 >/dev/null 2>&1; then
        QEMU_BIN="qemu-x86_64"
    elif command -v qemu-x86_64-static >/dev/null 2>&1; then
        QEMU_BIN="qemu-x86_64-static"
    else
        echo "qemu-x86_64 is not installed in this image" >&2
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

set +e
# Use qemu emulation for arm64 architecture
if [ "$ARCH" = "aarch64" ] || [ "$ARCH" = "arm64" ]; then
    "$QEMU_BIN" -L /usr "$WINE_BIN" /app/skp2gltf.exe "$@"
else
    "$WINE_BIN" /app/skp2gltf.exe "$@"
fi
EXIT_CODE="$?"
set -e

exit "$EXIT_CODE"
