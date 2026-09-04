---
title: Package indexes
order: 8
---

When you run `spn add lua`, a version appears from the mist. Where does it come from? And what happens when I want to maintain my own versions of Lua, or some private package?

## Directory indexes

A *package index* is just metadata about what packages exist, and what versions are available. The simplest possible index is just a directory of packages:

```
packages
├── flecs
│   └── spn.toml
├── fmt
│   └── spn.toml
├── imgui
│   └── spn.toml
├── sdl2
│   ├── spn.c
│   └── spn.toml
├── sdl2_mixer
│   └── spn.toml
└── tracy
    └── spn.toml
```

Point your build at it like this, or enable it globally in `~/.config/spn/spn.toml`:

```toml
[[index]]
name = "randy"
path = "./packages"
```

If you only care about one version existing at a time, this is all you need. If you just want to build your personal libraries, or use some common packages in different projects, you're done.

## Git indexes

Let's say you have a project that uses `sqlite==3.51.0`. You like it. But then, Richard Hipp returns from an ayahuasca retreat having had a vision: `4.0`. You can prompt an LLM from *inside* a query, to say the least.

You want to start a new project and use it, but you still need `3.51.0`. You've graduated to a second directory. This time, it's a directory of JSONL metadata:

```
index
└── sqlite.jsonl
```

Each JSONL entry is a release specifying the version, where the source code is, where the manifest is, etc.

```json
{
  "name": "sqlite",
  "version": "4.0.0",
  "yanked": false,
  "source": {
    "url": "https://github.com/sqlite/sqllm.git",
    "rev": "a1de0694c693a5d741b2b424e5dfea45eaa30e69"
  },
  "manifest": {
    "url": "https://github.com/you/manifests.git",
    "rev": "c7faa527fd",
    "dir": "sqlite"
  },
  "paths": {
    "manifest": "spn.toml",
    "script": "spn.c"
  }
}
```

Note the `manifest` field. If `sqllm` decided that `spn` is the tool of the future, and decided to have a *first party* manifest that lives in their repo, this field would point to `sqlite/sqllm.git`. But since we're writing a package for a third party library that doesn't know we exist, we point it at a repository that *we* own. These entries are created with `spn publish`, not by hand.

Use the index the same way as the simpler one:

```toml
[[index]]
name = "randy"
url = "git@github.com:you/index.git"
```

Let's dig into that a little more, because it's important: **Package indexes are just Git repositories**. You're going to *really* like this:
  - Commits are transactional, reversible, and everything else Git does for your source code
  - It comes with provenance and auth that you already have set up
  - Every god damn developer on the planet knows how to use it

If you've ever used Homebrew and waited multiple tens of seconds for kegs to be sterilized, you may balk at this. "Spader," you might say, "Git doesn't scale for package indexes". And you'd be right! But you'd also be wrong:
- `spn` isn't tied to Git. Index backends are just interfaces; there's a stubbed out HTTP backend in there already if you want to host it on Cloudflare or whatever
- C is not JS. It's not a distro, it's not Rust. Nobody uses packages in C because there are no good package managers for all its special little quirks. We're talking orders (plural) of magnitude less scale.

## Global configuration

## Publishing

### spn publish

### Namespaces

## The ecosystem

Here's some of the packages which have native, high quality `spn` manifests. The full list is in [the package manifest repository](https://github.com/tspader/spam). If a package is on this list, it has full support. That means you can cross compile it, pull in its optional dependencies, build its examples, and so on. Everything.
- [Clay](https://github.com/nicbarker/clay)
- [Dear ImGui](https://github.com/ocornut/imgui)
- [flecs](https://github.com/SanderMertens/flecs)
- [fmt](https://github.com/fmtlib/fmt)
- [FreeType](https://github.com/freetype/freetype)
- [Lua](https://github.com/lua/lua)
- [mbedtls](https://github.com/Mbed-TLS/mbedtls)
- [ninja](https://github.com/ninja-build/ninja)
- [raylib](https://github.com/raysan5/raylib)
- [SDL2](https://github.com/libsdl-org/SDL)
- [SDL2_mixer](https://github.com/libsdl-org/SDL_mixer)
- [SDL3](https://github.com/libsdl-org/SDL)
- [SDL3_image](https://github.com/libsdl-org/SDL_image)
- [SDL3_shadercross](https://github.com/libsdl-org/SDL_shadercross)
- [SDL3_ttf](https://github.com/libsdl-org/SDL_ttf)
- [sp.h](https://github.com/tspader/sp) :^)
- [SPIRV-Cross](https://github.com/KhronosGroup/SPIRV-Cross)
- [stb](https://github.com/nothings/stb)
- [Tracy](https://github.com/wolfpld/tracy)
- [yyjson](https://github.com/ibireme/yyjson)
