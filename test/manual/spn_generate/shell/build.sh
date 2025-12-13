#!/bin/bash
set -e
cd "$(dirname "$0")"

clean() {
  rm -rf main spn.sh spn.lock build/ main.c spn.toml
}

build() {
  clean
  cp ../main.c ../spn.toml .
  spn build
  spn generate --generator shell --path .
  source ./spn.sh
  gcc main.c -o main $SPN_FLAGS
  ./main
}

case "${1:-}" in
  clean) clean ;;
  *) build ;;
esac
