#!/usr/bin/env sh
set -eu

# >>> spn release data
VERSION="0.0.0"
TAG="v0.0.0"
REPO="tspader/spn"
TARGETS=""
# <<< spn release data

BASE_URL="${SPN_INSTALL_DOWNLOAD_URL:-https://github.com/${REPO}/releases/download/${TAG}}"

fail() {
  printf 'install: %s\n' "$1" >&2
  exit 1
}

download_curl() { curl -fSsL -o "$2" "$1"; }
download_curl_tty() { curl -fSL --progress-bar -o "$2" "$1"; }
download_wget() { wget -q -O "$2" "$1"; }
checksum_sha256sum() { sha256sum "$1" | cut -d' ' -f1; }
checksum_shasum() { shasum -a 256 "$1" | cut -d' ' -f1; }

if [ $# -gt 0 ]; then
  fail "the spn installer takes no arguments; configure it with SPN_INSTALL, SPN_INSTALL_DOWNLOAD_URL, and SPN_INSTALL_NO_MODIFY_PATH"
fi

if [ "${OS:-}" = "Windows_NT" ]; then
  exec powershell -NoProfile -Command "irm '${BASE_URL}/install.ps1' | iex"
fi

case "$(uname -s)" in
  Linux) SYS=linux ;;
  Darwin) SYS=macos ;;
  *) fail "unsupported operating system $(uname -s)" ;;
esac
case "$(uname -m)" in
  x86_64|amd64) CPU=x86_64 ;;
  aarch64|arm64) CPU=aarch64 ;;
  *) fail "unsupported architecture $(uname -m)" ;;
esac
if [ "$SYS" = "macos" ] && [ "$(sysctl -n hw.optional.arm64 2>/dev/null || echo 0)" = "1" ]; then
  CPU=aarch64
fi
TARGET="${CPU}-${SYS}"

ASSET=""
SHIPS=""
while read -r name asset sha exe; do
  if [ -z "$name" ]; then
    continue
  fi
  SHIPS="${SHIPS} ${name}"
  if [ "$name" = "$TARGET" ]; then
    ASSET="$asset"
    SHA="$sha"
    EXE="$exe"
  fi
done <<EOF
$TARGETS
EOF
if [ -z "$ASSET" ]; then
  fail "spn ${VERSION} has no build for ${TARGET}; it ships for:${SHIPS}"
fi

for tool in mktemp rm cut tar; do
  command -v "$tool" >/dev/null 2>&1 || fail "$tool is required to install spn"
done

if command -v curl >/dev/null 2>&1; then
  if [ -t 2 ]; then
    DOWNLOAD=download_curl_tty
  else
    DOWNLOAD=download_curl
  fi
elif command -v wget >/dev/null 2>&1; then
  DOWNLOAD=download_wget
else
  fail "curl or wget is required to install spn"
fi

if command -v sha256sum >/dev/null 2>&1; then
  CHECKSUM=checksum_sha256sum
elif command -v shasum >/dev/null 2>&1; then
  CHECKSUM=checksum_shasum
else
  fail "sha256sum or shasum is required to install spn"
fi

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

printf 'install: downloading spn %s (%s)\n' "$VERSION" "$TARGET"
"$DOWNLOAD" "${BASE_URL}/${ASSET}" "${TMP}/${ASSET}" || fail "failed to download ${BASE_URL}/${ASSET}"

GOT="$("$CHECKSUM" "${TMP}/${ASSET}")"
if [ "$GOT" != "$SHA" ]; then
  fail "sha256 mismatch for ${ASSET} (got ${GOT}, want ${SHA}); if a release is being published right now, retry in a minute"
fi

tar -xzf "${TMP}/${ASSET}" -C "$TMP" || fail "failed to extract ${ASSET}"
"${TMP}/${EXE}" self install
