# kernel

A bare metal image: no OS, no libc, no startup files. The manifest targets `freestanding` with `abi = "none"`, so the package supplies `_start`, a stack, and a linker script that places the image at `0x400000`.

```sh
spn build
spn build --target aarch64-freestanding-none
```
