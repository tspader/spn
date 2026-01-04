#!/bin/sh
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

for combo in alpine-musl alpine-glibc debian-musl debian-glibc; do
    echo "Building spn-$combo..."
    docker build -t "spn-$combo" "$SCRIPT_DIR/$combo"
done

echo ""
echo "Images built:"
echo "  spn-alpine-musl   (Alpine + musl)"
echo "  spn-alpine-glibc  (Alpine + glibc)"
echo "  spn-debian-musl   (Debian + musl)"
echo "  spn-debian-glibc  (Debian + glibc)"
