#!/usr/bin/env sh
set -eu

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
FAILED=0

check() {
  if [ "$3" -ne "$2" ]; then
    echo "FAIL $1 (rc $3, want $2)"
    FAILED=1
  else
    echo "ok   $1"
  fi
}

gate() {
  jq --exit-status --from-file "$ROOT/.github/ci-gate.jq" > /dev/null <<EOF
{"build":{"result":"$1"},"cold":{"result":"$2"},"package":{"result":"$3"},"canary":{"result":"$4"}}
EOF
}

while read -r build cold package canary expect; do
  rc=0
  gate "$build" "$cold" "$package" "$canary" || rc=1
  check "gate $build $cold $package $canary" "$expect" "$rc"
done <<EOF
success skipped success skipped 0
success skipped success success 0
failure success success success 0
failure success success skipped 0
failure failure skipped skipped 1
failure skipped skipped skipped 1
success skipped failure skipped 1
failure success success failure 1
cancelled skipped skipped skipped 1
EOF

STUB="$(mktemp -d)"
trap 'rm -rf "$STUB"' EXIT
cat > "$STUB/gh" <<'EOF'
#!/usr/bin/env sh
case "$1 $2" in
  "run list") printf '%s' "$GREENLIT_RUN" ;;
  "run view") printf '%s' "$GREENLIT_CONCLUSION" ;;
esac
EOF
chmod +x "$STUB/gh"

while read -r run conclusion expect; do
  [ "$run" = "-" ] && run=""
  [ "$conclusion" = "-" ] && conclusion=""
  rc=0
  PATH="$STUB:$PATH" GREENLIT_RUN="$run" GREENLIT_CONCLUSION="$conclusion" \
    sh "$ROOT/tools/greenlit.sh" ci 0000000 > /dev/null 2>&1 || rc=1
  check "greenlit run='$run' conclusion='$conclusion'" "$expect" "$rc"
done <<EOF
123 success 0
- - 1
123 failure 1
123 - 1
123 cancelled 1
EOF

rc=0
sh "$ROOT/tools/stage0.sh" --version | grep -Eq '^[0-9]+\.[0-9]+\.[0-9]+$' || rc=1
check "stage0 --version" 0 "$rc"

exit $FAILED
