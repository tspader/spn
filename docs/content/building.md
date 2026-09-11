---
title: Building
order: 5
---

`spn build` compiles your project and all dependencies. Compilation targets a *triple* (e.g. `x86_64-windows-gnu`) and uses a *profile* (e.g. release, O2, statically linked).

## Build output

By default, spn outputs to `build/`. If you explicitly pass a triple, it uses `build/triple/profile`. Otherwise, it uses `build/profile`. If you were to run this:

```bash
spn build
spn build --target x86_64-windows-gnu
```

It would produce this:

```
build/
├── debug/
└── x86_64-windows-gnu/
    └── debug/
```

Inside a given build, executables are placed at the top level for easy running. All other artifacts (headers, libraries, your dependencies' artifacts) are placed in `store/` in the usual way. Finally, a JSONL file with a detailed trace of the build is in `.spn/build.jsonl`

```
build/debug/
├── store/
│   ├── bin/
│   │   ├── main.exe
│   ├── lib/
│   │   ├── libwhatever.a
│   ├── include/
│   │   ├── whatever.h
│   │   └── ...
└── .spn/
    └── build.jsonl
```

## Selecting targets

### Profiles

Packages are compiled against a profile, which contains the target triple, toolchain, build mode, optimization level, sanitizers, etc. `spn` bakes in two profiles, `debug` and `release`, and reserves `[profile.default]` as the base fields which every profile uses unless overridden.

```toml
[profile.default]
toolchain = "zig"
linkage = "static"
standard = "c11"
mode = "debug"

[profile.debug]
opt = "2"

[profile.foo]
abi = { value = "gnu", when = { os = "linux" } }
toolchain = [
  { value = "clang", when = { os = "macos" } },
  { value = "gcc" },
]
```

Here, `debug` applies optimization on top of the default profile, and `foo` overrides the toolchain and ABI. Lists are tried in order such that the first match wins and an ungated entry is the fallback.

When building:
- `spn build` uses `debug`
- `spn build --mode release` uses `release`
- `spn build --profile NAME` uses the named `[[profile]]`

### Modes and optimization

### Sanitizers

## Cross compilation

### Target triples

### --os, --arch, --abi

## CI

`spn` was built for CI from day one:
- The build cache is designed to scale to your entire team
- First class cross compilation destroys your insane matrix of runners
- Out of the box, zero configuration integrations with GitHub Actions
- The CLI can output a structured JSONL stream. You never have to scrape an error message
- If it doesn't work how you want, write your own CLI that links to `libspn` instead
- @spader Are these weak? People care about fast builds mostly but we got that elsewhere...

## Caching

spn has a built in build cache which caches any intermediate artifact that your build creates, like `ccache` or `sccache`. Builds are incremental by default. That doesn't mean "incremental on my machine". That means *incremental*. Build `foo.exe` on one machine, and the cache is designed such that *any* subsequent machine building `foo.exe` can see a fully cached build.

That's because, at its core, `spn` is a *content addressed DAG*. If you've never seen the terms, let's take a ride!

### Incremental builds

This means that everything in your build is turned into nodes in a graph. If something in the middle of the graph changes, we know exactly what needs to be rebuilt and in what order. This is like every other incremental build system that has ever existed.

### Content addressing

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

### --force

## Running tests

## Output

### compile_commands.json

### JSON event stream
