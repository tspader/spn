# examples

Every directory is a standalone example. Run it like this:

```sh
cd lua
spn build
./build/debug/main
```

The windowed examples (`raylib`, `sdl3`, `imgui`) need a profile from their manifest on Linux and macOS:

```sh
spn build -p linux
spn build -p macos
spn build
```

The headless examples cross compile from any host:

```sh
spn build --target x86_64-windows-gnu
spn build --target aarch64-macos
```

On Windows, add `--toolchain msvc` from a Visual Studio developer shell to build with MSVC instead of zig.

The bare metal examples (`kernel`, `nolibc`) pick their target in the manifest, so a plain `spn build` cross compiles; `nolibc` runs on a Linux host:

```sh
cd nolibc
spn build && ./build/x86_64-linux-none/debug/main
```
