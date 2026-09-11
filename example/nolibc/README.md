# nolibc

A Linux binary with no libc and no startup files. The manifest sets `abi = "none"`, so `main.c` supplies `_start` and talks to the kernel with raw syscalls.

```sh
spn build && ./build/x86_64-linux-none/debug/main
```
