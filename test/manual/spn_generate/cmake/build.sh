#!/bin/bash
set -e
cd "$(dirname "$0")"

clean() {
  rm -rf build spn.cmake spn.lock main.c spn.toml
}

build() {
  clean
  cp ../main.c ../spn.toml .
  spn build
  spn generate --generator cmake --path .
  cmake -B build
  cmake --build build
  ./build/main
}

case "${1:-}" in
  clean) clean ;;
  *) build ;;
esac
