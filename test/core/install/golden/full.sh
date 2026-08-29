#!/usr/bin/env sh
# shellcheck disable=SC2016
set -eu

VERSION="0.0.0"
TAG="v0.0.0"
BASE_URL="${SPN_INSTALL_DOWNLOAD_URL:-https://github.com/A/B/releases/download/${TAG}}"

fail() {
  printf 'install: %s\n' "$1" >&2
  exit 1
}

download_curl() { curl -fSsL -o "$2" "$1"; }
download_curl_tty() { curl -fSL --progress-bar -o "$2" "$1"; }
download_wget() { wget -q -O "$2" "$1"; }
checksum_sha256sum() { sha256sum "$1" | cut -d' ' -f1; }
checksum_shasum() { shasum -a 256 "$1" | cut -d' ' -f1; }

write_env_sh() {
  printf 'case ":${PATH}:" in\n  *:"%s":*) ;;\n  *) export PATH="%s:${PATH}" ;;\nesac\n' "$1" "$1" > "$2"
}

write_env_fish() {
  printf 'if not contains "%s" $PATH\n  set --export PATH "%s" $PATH\nend\n' "$1" "$1" > "$2"
}

ensure_rc_line() {
  if [ -f "$1" ] && grep -Fq "$RC_LINE" "$1"; then
    return 0
  fi
  printf '\n%s\n' "$RC_LINE" >> "$1" || fail "failed to update $1"
}

rc_files() {
  printf '%s\n' "${HOME}/.profile"
  for rc in "${HOME}/.bashrc" "${HOME}/.bash_profile" "${HOME}/.bash_login"; do
    if [ -f "$rc" ]; then
      printf '%s\n' "$rc"
    fi
  done
  printf '%s\n' "${ZDOTDIR:-$HOME}/.zshrc"
}

setup_path() {
  write_env_sh "${INSTALL_EXPR}/bin" "${SPN_INSTALL}/env"
  mkdir -p "${HOME}/.config/fish/conf.d"
  write_env_fish "${INSTALL_EXPR}/bin" "${HOME}/.config/fish/conf.d/spn.fish"
  rc_files | while IFS= read -r rc; do
    ensure_rc_line "$rc"
  done
}

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

case "$TARGET" in
  aarch64-macos) ASSET="spn-aarch64-macos.tar.gz" SHA="aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa" EXE="spn" ;;
  x86_64-linux) ASSET="spn-x86_64-linux.tar.gz" SHA="bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb" EXE="spn" ;;
  *) fail "spn ${VERSION} has no build for ${TARGET}; it ships for: aarch64-macos x86_64-linux" ;;
esac

if [ -z "${SPN_INSTALL:-}" ] && [ -z "${HOME:-}" ]; then
  fail "HOME is not set; set SPN_INSTALL to choose an install directory"
fi
if [ -n "${SPN_INSTALL:-}" ]; then
  case "$SPN_INSTALL" in
    *'"'*|*'$'*|*'`'*|*\\*) fail 'SPN_INSTALL may not contain a quote, dollar sign, backtick, or backslash' ;;
  esac
  INSTALL_EXPR="$SPN_INSTALL"
else
  SPN_INSTALL="${HOME}/.spn"
  INSTALL_EXPR='$HOME/.spn'
fi
BIN_DIR="${SPN_INSTALL}/bin"
RC_LINE=". \"${INSTALL_EXPR}/env\""

for tool in mktemp mkdir mv chmod rm grep cut tar; do
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
STAGE=""
trap 'rm -rf "$TMP" ${STAGE:+"$STAGE"}' EXIT

printf 'install: downloading spn %s (%s)\n' "$VERSION" "$TARGET"
"$DOWNLOAD" "${BASE_URL}/${ASSET}" "${TMP}/${ASSET}" || fail "failed to download ${BASE_URL}/${ASSET}"

GOT="$("$CHECKSUM" "${TMP}/${ASSET}")"
if [ "$GOT" != "$SHA" ]; then
  fail "sha256 mismatch for ${ASSET} (got ${GOT}, want ${SHA}); if a release is being published right now, retry in a minute"
fi

tar -xzf "${TMP}/${ASSET}" -C "$TMP" || fail "failed to extract ${ASSET}"

mkdir -p "$BIN_DIR"
STAGE="$(mktemp -d "${BIN_DIR}/.stage.XXXXXX")"
mv "${TMP}/${EXE}" "${STAGE}/${EXE}"
chmod +x "${STAGE}/${EXE}"
mv -f "${STAGE}/${EXE}" "${BIN_DIR}/${EXE}" || fail "failed to replace ${BIN_DIR}/${EXE}; close any running spn and retry"

VERSION_OUT="$("${BIN_DIR}/${EXE}" --version)" || fail "the installed spn failed to run"

PATH_STATE=ok
case ":${PATH}:" in
  *:"${BIN_DIR}":*) ;;
  *)
    if [ -n "${SPN_INSTALL_NO_MODIFY_PATH:-}" ]; then
      PATH_STATE=manual
    elif [ -n "${GITHUB_PATH:-}" ]; then
      printf '%s\n' "$BIN_DIR" >> "$GITHUB_PATH"
      write_env_sh "${INSTALL_EXPR}/bin" "${SPN_INSTALL}/env"
      PATH_STATE=ci
    elif [ -z "${HOME:-}" ]; then
      PATH_STATE=manual
    else
      setup_path
      PATH_STATE=updated
    fi
    ;;
esac

printf 'install: %s installed to %s\n' "$VERSION_OUT" "${BIN_DIR}/${EXE}"
case "$PATH_STATE" in
  ok|ci) ;;
  updated)
    printf 'install: restart your shell, or run:\n'
    printf 'install:   . "%s/env"\n' "$INSTALL_EXPR"
    ;;
  manual)
    printf 'install: add %s to your PATH\n' "$BIN_DIR"
    ;;
esac

SHADOW="$(command -v spn 2>/dev/null || echo "")"
case "$SHADOW" in
  ""|"${BIN_DIR}/spn"|"${BIN_DIR}/${EXE}") ;;
  *) printf 'install: another spn at %s shadows %s\n' "$SHADOW" "${BIN_DIR}/${EXE}" >&2 ;;
esac
