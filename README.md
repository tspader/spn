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

# installation

```sh
curl -fsSL https://spn.spader.zone/install | sh
```

On Windows:

```powershell
irm https://spn.spader.zone/install.ps1 | iex
```

# quickstart

Let's use the following program, which needs to link to Lua, as an example:

```c
#include <stdio.h>
#include "lauxlib.h"
#include "lualib.h"

int main(void) {
  lua_State *L = luaL_newstate();
  luaL_openlibs(L);
  if (luaL_dofile(L, NULL) != LUA_OK) {
    fprintf(stderr, "%s\n", lua_tostring(L, -1));
    return 1;
  }
  lua_close(L);
  return 0;
}
```

First, initialize your project. We'll pull Lua from [the default package index](https://github.com/tspader/spam) for now; later, we'll talk about making your own packages.

```sh
spn init
spn add lua
```

Those commands create a project manifest, `spn.toml`, which defines a single `[[bin]]` executable.

```toml
[package]
name = "demo"
version = "0.1.0"

[[bin]]
name = "demo"
source = ["main.c"]

[deps.package]
lua = "5.4.6"

```

Then, after copying the source code into `main.c`:

```sh
spn build
./build/debug/demo
```

Cross compiling works out of the box. The following commands produce `build/x86_64-windows-gnu/debug/demo.exe` and `build/aarch64-macos-apple/debug/demo`:

```sh
spn build --target x86_64-windows-gnu
spn build --target aarch64-macos
```

Bare metal works the same way. A program with no libc and its own entry point:

```c
void _start(void) {
  for (;;) {
  }
}
```

builds into a static `build/aarch64-freestanding-none/debug/demo.elf` with no startup files:

```sh
spn build --target aarch64-freestanding
```

A kernel image needs its own memory layout. Give the target a linker script and any raw flags the link needs:

```toml
[[bin]]
name = "kernel"
source = ["kernel.c", "start.S"]
linker_script = ["kernel.ld"]
link_flags = ["-Wl,--build-id=none", "-Wl,-n"]
```

Congratulations! You now have a C program which:
- Can be compiled to any OS, architecture, and ABI
- Builds extremely quickly and incrementally
-

# table of contents
- [Overview](#overview)
- [Packages](#packages)
  - [Targets](#targets)
  - [Profiles](#profiles)
  - [Platform configuration](#configure-stuff-per-platform)
  - [Embedding stuff in your binary](#embed-bytes-in-a-binary)
- [Builds](#builds)
  - [As fast as possible](#this-is-not-a-wrapper)
  - [Toolchains](#toolchains)
- [CI and build caches](#ci)
- [Dependencies](#dependencies)
  - [Package Indexes](#package-indexes-are-git-repos)
  - [Publishing](#git-is-all-you-need-for-rich-private-packages)
- [Build Scripts](#build-scripts)
  - [Build Dependencies](#build-scripts-can-have-dependencies)
  - [API](#build-scripts-have-access-to-the-api)
  - [Hermetic, sandboxed](#build-scripts-are-hermetic-and-sandboxed)
- [Embedding `spn`](#embedding-spn)
  - [There's a stable C ABI](#theres-a-stable-c-abi)
  - [Integration](#it-works-with-any-event-loop)
  - [Hotloading](#its-so-much-better-at-hotloading-than-your-crappy-script)
- [Workspaces](#workspaces)
  - [Task runner](#spn-is-a-task-runner)
  - [Test runner](#spn-is-a-test-runner)
- [Why do I care?](#why-do-i-care)
  - ["My CI isn't absurdly fast with zero configuration"](#my-ci-isnt-absurdly-fast-with-zero-configuration)
  - ["My company doesn't approve tools with dependencies (e.g. Python or JS)"](#my-company-doesnt-approve-tools-with-dependencies-eg-python-or-js)
- [Development](#development)
  - [Building](#building)
  - [Testing](#testing)
  - [PRs](#prs)

# overview

Building C code should be this easy, always. This demo is just a fraction of the functionality `spn` offers. It can replace nearly every tool in what is today a patchwork ecosystem of developer tooling:
- Build tools like CMake, Make, and Ninja
- Package managers like Conan, vcpkg, pkg-config, or "whatever's on my distro"
- Build caches like ccache or sccache
- Binary tools like xxd, patchelf, and objcopy
- Compilation database tooling like bear or compiledb
- Toolchain managers like crosstool-NG, dockcross, CMake toolchain files

Let's take a look at everything it can do in more detail!

# packages
Packages are defined by TOML manifests.

## targets

A package can define as many executables, libraries, tests, and examples as it would like. These are called *targets*. They have fields like `source`, or `include`, or `system_deps`, and most fields can be configured at the package level for all targets or for an individual target.

```toml
[package]
include = ['common']

[[lib]]
source = ['source/lib.c']
include = ['source/lib']

[[example]]
source = ['example/foo/main.c']

[[test]]
source = ['test/main.c', 'test/foo.c']
define = ['SOMETHING']
system_deps = ['m']
```

## profiles

Packages are compiled against a profile, which contains the target triple, toolchain, build mode, optimization level, sanitizers, etc.

```toml
[profile.default]
toolchain = "zig"
linkage = "static"
standard = "c11"
mode = "debug"
```

## conditional configuration

Any field in your package can be keyed on any fact of the build (target OS, architecture, ABI, compiler driver, build mode, optimization level, sanitizer settings). Clauses are structured data, not a DSL, and an entry with multiple clauses gets them ANDed together. Here's some common examples:

```toml
source = [
  { path = "source/backend/win32.c", when = { os = "windows" } },
  { path = "source/backend/posix.c", when = { os = { not = "windows" } } },
]
flags = [
  { value = "-mfpu=neon", when = { os = "linux", arch = "aarch64" } },
  { value = "/W4", when = { driver = "msvc" } },
]
define = [
  { value = "USE_DEBUG_ALLOC", when = { mode = "debug", sanitize_address = false } },
]
system_deps = [
  { value = "pthread", when = { os = { not = "windows" } } },
]
deps = [
  { pkg = "tracy", when = { mode = "debug" } },
]
```

Options declared in the manifest work as keys, too, so a feature flag can gate sources and dependencies together:

```toml
[options.freetype]
type = "bool"
default = true
define = "UI_FREETYPE"

[[lib]]
name = "ui"
source = [
  { path = "source/text_freetype.c", when = { freetype = true } },
]
deps = [
  { pkg = "freetype", when = { freetype = true } },
]
```

## platform specific configuration

### macOS configuration

```toml
[package.macos]
min_os = { major = 12 }

[lib.macos]
frameworks = ["Cocoa", "IOKit", "CoreVideo", "OpenGL"]
```

When linking, your binary's `min_os` is the max across everything in it. If a dependency needs macOS 12, your binary targets macOS 12.

### windows configuration

```toml
[[bin]]
# ...
windows = { subsystem = "windows" }
```


## embed bytes in a binary

`spn` can embed arbitrary files and bytes (from build scripts) in your binary by creating an object file and header that anything can link to trivially. For example, this configuration:

```toml
[[bin]]
name = "whatever"
source = ["main.c"]
embed = [
  "asset/fonts/inconsolata.ttf",
  { path = "asset/data.json", symbol = "data_json", data_type = "u8", size_type = "u64" },
  { path = "asset/shaders", dir = true, dest = "shaders" },
]
```

Produces this header:

```c
extern const unsigned char asset_fonts_inconsolata_ttf [109384];
extern const unsigned long long asset_fonts_inconsolata_ttf_size;

extern const u8 data_json [2213];
extern const u64 data_json_size;

extern const unsigned char shaders_quad_frag [412];
extern const unsigned long long shaders_quad_frag_size;

extern const unsigned char shaders_quad_vert [287];
extern const unsigned long long shaders_quad_vert_size;

typedef struct {
  const char* path;
  const void* data;
  unsigned long long size;
} spn_embed_entry_t;
static const unsigned int spn_embed_count = 4;
static const spn_embed_entry_t spn_embed_manifest[] = {
  { "asset/fonts/inconsolata.ttf", asset_fonts_inconsolata_ttf, 109384 },
  { "asset/data.json", data_json, 2213 },
  { "shaders/quad.frag", shaders_quad_frag, 412 },
  { "shaders/quad.vert", shaders_quad_vert, 287 },
};
```

# builds

## this is not a wrapper

## toolchains
### bring your own toolchain

### (or feel the warm embrace of `zig cc`)

# CI

`spn` was built for CI from day one:
- The build cache is designed to scale to your entire team
- First class cross compilation destroys your insane matrix of runners
- Out of the box, zero configuration integrations with GitHub Actions
- The CLI can output a structured JSONL stream. You never have to scrape an error message
- If it doesn't work how you want, write your own CLI that links to `libspn` instead
- @spader Are these weak? People care about fast builds mostly but we got that elsewhere...

## everything is cached

spn has a built in build cache which caches any intermediate artifact that your build creates, like `ccache` or `sccache`. Builds are incremental by default. That doesn't mean "incremental on my machine". That means *incremental*. Build `foo.exe` on one machine, and the cache is designed such that *any* subsequent machine building `foo.exe` can see a fully cached build.

That's because, at its core, `spn` is a *content addressed DAG*. If you've never seen the terms, let's take a ride!

### spn is a dag

This means that everything in your build is turned into nodes in a graph. If something in the middle of the graph changes, we know exactly what needs to be rebuilt and in what order. This is like every other incremental build system that has ever existed.

### spn is content addressed

If you use CMake, you've probably encountered this:

```sh
# Build once. After this, builds are incremental. You are happy.
make

# Ah, but you gotta check out another branch
git checkout whatever
git checkout main

# This is a full rebuild, because Git touched every file and made it
# appear to have been edited since the last build. You are sad.
make
```

That's because CMake, and in fact most build systems, lie to you. They tell you that they know when `foo.c` changed. But they have no god damn idea! None whatsoever! Now, they have *proxies* which in practice are...fine. Like, for example, a file's mtime. Last build was at 3:00, `foo.c` says 3:05, let's rebuild it.

But files get touched *all the time*! For no reason! Sometimes, they even go *backwards*, like when you decompress an archive, and then your build is wrong instead of just slow. This is the first problem: Traditional build systems have trouble knowing when something changed.

There's a beautiful solution to both of these things at once! If you're having trouble giving everything a unique identity, and you're having trouble figuring out when a given thing changed, *make their identity be their content*. When we build `foo.o`, we hash its bytes. Let's say that hash comes out to, miraculously, `0x69`. Now, there's no such thing as `foo.o`; there's just a file called `0x69` in the cache.

That's great, but the next build still needs to be able to know that, ah, yes, we need the cache entry keyed at `0x69`. To do this, we look at all of the inputs to `foo.o`:
- `foo.c`, of course
- Let's say that the compiler reported that `stdint.h` was used, too

We hash their content too, and get `0x420` and `0x5F3759DF`. Then, all we do is write down a fact:

> If the inputs to the compiler are `0x420` and `0x5F3759DF`, then the output will be `0x69`

Next time, when we're ready to build `foo.o`, we have all of its inputs ready to go. Take those inputs, and ask the fact machine if it knows the answer for that set of inputs. If `foo.c` was edited, the inputs are no longer (`0x420`, `0x5F3759DF`). If `spum.h` got added to the build, then there are now three inputs rather than two. And, if the inputs are the same but `0x69` isn't in the cache, all you have to do is rebuild!

This is beautiful. Identity is content; content is identity. A file doesn't have a name. It simply *is*. There are, of course, many kinks to work out in such a system, but they're all tractable.

This is the exact principle behind Bazel, BuildXL, and Nix, and it's the fundamental reason why `spn` is so good at caching your builds across machines.

# dependencies

`spn` is a real package manager, built specifically for C and C++.

## local dependencies

The simplest dependency is a path to a directory with an `spn.toml`:

```toml
[deps.package]
foo = { path = "packages/foo" }
```

And this is what the package might look like:

```toml
[package]
name = "foo"
version = "6.9.0"

[[lib]]
name = "foo"
source = ["foo.c"] # Relative to foo's manifest
```

This is just an old fashioned vendored dependency. You could download LLVM, check in the entire source tree, and have an `spn.toml` that builds this local copy. If you then regained your sanity, you can point the exact same manifest at Git and `spn` will manage the checkout for you *exactly* as if it were an "official" package:

```toml
[package.upstream]
url = "https://github.com/tspader/foo.git"
commit = "6937fa02243da7b693c5692cea84a696950d4669"
```

## patching dependencies

What if you need to patch `foo`? You don't want to go back to the technology of the ancients, like submodules or vendoring. You want a specific commit with just a few changes. Do this in your `spn.toml`:

```toml
[deps.package]
foo = "6.9.0"

[patch.foo]
files = ["patches/foo.patch"]
```

That's it! Patches fold into the build system like everything else. If you change the contents of a patch, add or remove patches, you'll still get a correct, incremental build.

Your dependencies can't patch. If you ask for a given commit, you get exactly that commit.

## configuration

Packages provide options. Options can be enumerations or booleans; enums are mutually exclusive, and an unresolvable conflict is a build error. Booleans are additive. A package `foo` might declare them like this:

```toml
[options.tls]
type = "enum"
values = ["schannel", "openssl", "off"]
default = [
  { when = { os = "windows" }, value = "schannel" },
  { when = { os = { not = "wasi" } }, value = "openssl" },
  { value = "off" },
]

[options.zstd]
type = "bool"
default = false
```

Then, consumers of `foo` set them like this:

```toml
[config.foo]
tls = "openssl"
zstd = true
```

## linkage

Libraries declare the linkages they support:
- `shared`
- `static`
- `source`, which compiles the package's sources directly into your target as if they were your own files.

```toml
[config.foo]
kind = "shared"
```

## hosting an index is really easy

When you run `spn add lua`, a version appears from the mist. Where does it come from? And what happens when I want to maintain my own versions of Lua, or some private package?

### directory indexes

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

### git indexes

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

### git is all you need for rich, private packages

Let's dig into that a little more, because it's important: **Package indexes are just Git repositories**. You're going to *really* like this:
  - Commits are transactional, reversible, and everything else Git does for your source code
  - It comes with provenance and auth that you already have set up
  - Every god damn developer on the planet knows how to use it

If you've ever used Homebrew and waited multiple tens of seconds for kegs to be sterilized, you may balk at this. "Spader," you might say, "Git doesn't scale for package indexes". And you'd be right! But you'd also be wrong:
- `spn` isn't tied to Git. Index backends are just interfaces; there's a stubbed out HTTP backend in there already if you want to host it on Cloudflare or whatever
- C is not JS. It's not a distro, it's not Rust. Nobody uses packages in C because there are no good package managers for all its special little quirks. We're talking orders (plural) of magnitude less scale.

## the ecosystem

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

# build scripts

You need to run code in your build. Most people solve this by writing brittle scripts in Bash or Powershell, or, at best, a language like Python. `spn` solves this with WebAssembly[^1]. It embeds a WASM runtime, and will automatically compile arbitrary C programs to WASM modules that run in the build graph. You do not write code against a subset of C, or against a DSL. You write regular code, in the language you were using anyway:

```c

```

## build scripts can have dependencies

```toml
[deps.build]
```

```c

```


## build scripts have access to the API

```c

```

If you're porting over an existing build, you can continue to use what you have.

## build scripts are hermetic and sandboxed

# embedding `spn`

## there's a stable C ABI

## it works with any event loop

## it's so much better at hotloading than your crappy script

# workspaces

## `spn` is a task runner
## `spn` is a test runner

# why do i care?

## "My CI isn't absurdly fast with zero configuration"

Look. I get it. Setting up the mess of tooling and integrations needed to cache an object file reliably is hard. It's so much easier to just sink into that warm abyss of twenty minute builds (twenty minutes is normal, right?) and tell yourself it doesn't matter.

Twenty minutes is not normal! An hour is not normal! 90% of your builds in CI should be so fast that you have to double check that it actually happened.

If this isn't the case for whatever you work on, [send me an email right god damn now](mailto:admin@spader.zone)! Building native code is an *extremely well understood* problem. The problem's not that we don't know how, it's that we know how, and, well, it's pretty fucking hard unless you spend a *lot* of time on it. You, person who are presumably making and/or selling something other than an obscure meta-tool, don't have that much spare time[^2].

takes quite a bit of rigor and care that most folks who are actually making and selling things can't spare.

## "My company doesn't approve tools with dependencies (e.g. Python or JS)"

`spn` has *zero* dependencies. It is exactly one binary. Builds are hermetic and sandboxed by default, and it's as easy to pin the exact commits of your dependencies as it is to use packages from the index.

# can this replace...

## ...bear, compiledb?

Yes. `spn` generates `compile_commands.json` out of the box, with no instrumentation needed.

# development
## building
## testing
## PRs

[^1]: WebAssembly is a platform agnostic binary target; instead of compiling code for x86_64 or ARM64 machine code and running it with your CPU, you compile it to WASM bytecode and run it inside a regular program.

[^2]: And if you did, you'd probably spend it running your hands through the soft day-warmed grass of some shaded grove, those whom you love most by your side, feeling the beams of the sun against your face and basking in the simple pleasure of being alive.

