#!/bin/sh
set -e

PROFILE="${1:-debug}"
DISTRO="${2:-alpine}"
LIBC="${3:-musl}"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
BIN_DIR="$REPO_ROOT/build/$PROFILE/store/bin"

if [ ! -d "$BIN_DIR" ]; then
    echo "Error: $BIN_DIR does not exist" >&2
    exit 1
fi

IMAGE="spn-$DISTRO-$LIBC"

exec docker run --rm -it -v "$BIN_DIR:/spn/bin:ro" "$IMAGE" /bin/sh
