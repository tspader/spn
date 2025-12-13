#!/bin/bash
set -e
cd "$(dirname "$0")"

clean() {
  rm -rf main spn.pc spn.lock build/ main.c spn.toml
}

build() {
  clean
  cp ../main.c ../spn.toml .
  spn build
  spn generate --generator pkgconfig --path .
  gcc main.c -o main $(pkg-config --cflags --libs ./spn.pc)
  ./main
}

case "${1:-}" in
  clean) clean ;;
  *) build ;;
esac
