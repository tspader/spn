#!/bin/sh
# Manual smoke matrix for spn's linker/toolchain machinery.
#
#   sh test/smoke/run.sh              local + every docker lane, then a coverage matrix
#   sh test/smoke/run.sh docker       docker lanes only
#   sh test/smoke/run.sh local        the native host lane only
#   sh test/smoke/run.sh hosts        the POSIX hosts in hosts.txt (macOS etc.)
#   sh test/smoke/run.sh all          local + docker + hosts
#   sh test/smoke/run.sh <variant>    one docker variant by name
#
# Each lane runs the whole smoke binary with --filter '*'; cases self-gate, so a
# lane runs only the cells its toolchain supports and skips the rest. The final
# matrix folds every lane together: a cell is PROVEN if any lane linked it, FAIL
# if any lane failed it, else PENDING (only ever skipped -> no toolchain reached
# it here). Windows msvc cells are run by hand; see README.md.
set -e

DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$DIR/../.." && pwd)"
ARCH="$(uname -m)"; [ "$ARCH" = arm64 ] && ARCH=aarch64
MUSL="build/$ARCH-linux-musl/musl"
BIN="$MUSL/test/smoke"
GITDIR="$(cd "$(git -C "$ROOT" rev-parse --git-common-dir)" && pwd)"
TOOLCHAINS="$HOME/.local/share/spn/cache/toolchain"
ZIG_CACHE="$HOME/.cache/zig"
OUT="$(mktemp -d)"
mkdir -p "$TOOLCHAINS" "$ZIG_CACHE"

if [ ! -x "$ROOT/$BIN" ]; then
  echo "building musl smoke binary" >&2
  (cd "$ROOT" && spn build -p musl && spn build -p musl --test smoke)
fi

image_dir() {
  if [ -d "$DIR/variants/$1" ]; then echo "$DIR/variants/$1"
  else echo "$ROOT/test/tools/docker/variants/$1"; fi
}

# run one lane, tee its per-cell results into $OUT/<lane> for the matrix
lane() { # <name> <cmd...>
  name="$1"; shift
  echo "== $name"
  "$@" 2>&1 | grep -E '^(elf|mingw|msvc|macho|wasm)\.[a-z_]+ (ok|skipped|failed)' \
    | tee "$OUT/$name" | sed 's/^/   /' || true
}

run_local() {
  lane local env SPN_TEST_TOOLCHAIN=zig "$ROOT/$BIN" --filter '*'
}

run_docker() { # <variant> <toolchain>
  img="spn-smoke-$1"; d="$(image_dir "$1")"
  docker build -q -t "$img" "$d" >/dev/null
  lane "$1" docker run --rm --user "$(id -u):$(id -g)" \
    -e HOME="$HOME" -e "SPN_TEST_TOOLCHAIN=$2" \
    -v "$ROOT:$ROOT" -v "$GITDIR:$GITDIR" -v "$TOOLCHAINS:$TOOLCHAINS" -v "$ZIG_CACHE:$ZIG_CACHE" \
    -w "$ROOT" "$img" "$ROOT/$BIN" --filter '*'
}

docker_lanes() {
  while read -r variant toolchain _; do
    case "$variant" in ''|'#'*) continue ;; esac
    run_docker "$variant" "$toolchain"
  done < "$DIR/docker.txt"
}

run_host() { # <name> <ssh> <root> <triple> <toolchains...>
  name="$1"; sshv="$2"; hroot="$3"; triple="$4"; shift 4
  case "$triple" in *windows*) echo "== $name: run by hand (Windows); see README.md" >&2; return ;; esac
  if ! ssh -o ConnectTimeout=10 -o BatchMode=yes "$sshv" true 2>/dev/null; then
    echo "== $name: offline (ssh $sshv)" >&2; return
  fi
  prof=debug; bdir="build/$triple/$prof"
  (cd "$ROOT" && spn build --target "$triple" && spn build --target "$triple" --test smoke) >/dev/null
  tar -czf "$OUT/$name.tgz" -C "$ROOT" \
    $(git -C "$ROOT" ls-files) $(git -C "$ROOT" ls-files --others --exclude-standard -- test/smoke) \
    "$bdir/spn" "$bdir/test/smoke"
  ssh "$sshv" "mkdir -p '$hroot' && tar -xzf - -C '$hroot'" < "$OUT/$name.tgz"
  for tc in "$@"; do
    lane "$name:$tc" ssh "$sshv" "cd '$hroot' && SPN_TEST_TOOLCHAIN=$tc ./$bdir/test/smoke --filter '*'"
  done
}

host_lanes() {
  while read -r name sshv hroot triple rest; do
    case "$name" in ''|'#'*) continue ;; esac
    run_host "$name" "$sshv" "$hroot" "$triple" $rest
  done < "$DIR/hosts.txt"
}

matrix() {
  cells="$("$ROOT/$BIN" --list)"
  echo
  echo "== coverage matrix"
  printf '%-28s %s\n' "cell" "status (lane)"
  for cell in $cells; do
    verdict=PENDING; where=""
    for f in "$OUT"/*; do
      [ -f "$f" ] || continue
      line="$(grep "^$cell " "$f" 2>/dev/null || true)"
      case "$line" in
        *" failed"*) verdict=FAIL; where="$(basename "$f")"; break ;;
        *" ok "*|*" ok") [ "$verdict" = PENDING ] && { verdict=PROVEN; where="$(basename "$f")"; } ;;
      esac
    done
    printf '%-28s %s %s\n' "$cell" "$verdict" "$where"
  done
}

case "${1:-default}" in
  local)  run_local ;;
  docker) docker_lanes ;;
  hosts)  host_lanes ;;
  all)    run_local; docker_lanes; host_lanes ;;
  default) run_local; docker_lanes ;;
  *)      run_docker "$1" "$(awk -v v="$1" '$1==v{print $2}' "$DIR/docker.txt")" ;;
esac

[ "$1" = local ] || [ "$1" = hosts ] || matrix
rm -rf "$OUT"
