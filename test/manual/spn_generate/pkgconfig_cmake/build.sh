#!/bin/bash
set -e
cd "$(dirname "$0")"

clean() {
  rm -rf build spn.pc spn.lock main.c spn.toml
}

build() {
  clean
  cp ../main.c ../spn.toml .
  spn build
  spn generate --generator pkgconfig --path .
  PKG_CONFIG_PATH="$PWD" cmake -B build
  cmake --build build
  ./build/main
}

case "${1:-}" in
  clean) clean ;;
  *) build ;;
esac
