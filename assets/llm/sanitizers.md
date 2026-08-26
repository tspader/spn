# bootstrap
```sh
make SANITIZE=1 test
```

# self hosted
```sh
spn build --profile sanitizer
```

For the moment, a little workaround on MSVC:
```sh
spn build --profile msvc --sanitize address
```

We don't do x86_64-windows-gnu because Zig only supports UBSan there and CMake only builds MSVC

# coverage
- Linux: ASan + UBSan with Clang
- macOS: ASan + UBSan with Clang
- Windows: ASan with MSVC
