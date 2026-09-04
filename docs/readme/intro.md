<p align="center">
  <a href="https://spn.spader.zone">
    <picture>
      <source media="(prefers-color-scheme: dark)" srcset="assets/logo/logo-dark.svg">
      <img src="assets/logo/logo-light.svg" alt="spn" width="180">
    </picture>
  </a>
</p>

<p align="center">
  <a href="https://discord.gg/7v4C5Kwsp7">
    <img src="https://img.shields.io/discord/957469344974143508?style=flat-square&label=discord" />
  </a>
  <a href="https://github.com/tspader/spn/actions/workflows/ci.yml">
    <img src="https://img.shields.io/github/actions/workflow/status/tspader/spn/ci.yml?style=flat-square&branch=main" />
  </a>
</p>

`spn` is the missing everything tool for writing and building C code:
- An extremely fast build system with a built-in object cache that's designed for CI
- TOML manifests for defining projects
- A package manager that supports everything from "pin the world to exact commits" to "treat C like JS"
- Sandboxed, hermetic builds that never run arbitrary native code

It ships as a single file static executable, or an embeddable library with a stable C ABI. It runs natively on Linux, macOS, and Windows and it supports most common toolchains (MSVC, GCC, Clang, MinGW, Zig) and can cross compile to any target your toolchain supports.

If any part of your build pipeline would benefit from fast, cached, reproducible compilation of C code backed by tools that are obsessed with making your life easier, then you're in the right place!

**spn is in alpha**. Join [our Discord](https://discord.gg/7v4C5Kwsp7) if you'd like to chat, need help, or want to scream into the void that I'm wasting my life.
