---
title: Dependencies
---

`spn` is a real package manager, built specifically for C and C++.

## Local dependencies

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

## Conditional and private dependencies

## Configuration

### Options

Packages can [provide options](/docs/packages#options), either as an enumeration or a boolean. Options for `foo` are set in `[config.foo]`, like this:

```toml
[config.foo]
tls = "openssl"
zstd = true
```

### Linkage

Libraries declare the linkages they support:
- `shared`
- `static`
- `source`, which compiles the package's sources directly into your target as if they were your own files.

```toml
[config.foo]
kind = "shared"
```

## Patching

What if you need to patch `foo`? You don't want to go back to the technology of the ancients, vendoring or submoduling entire repositories just to change a few lines. You want a specific commit with just a few changes. Do this in your `spn.toml`:

```toml
[deps.package]
foo = "6.9.0"

[patch.foo]
files = ["patches/foo.patch"]
```

That's it! Patches fold into the build system like everything else. If you change the contents of a patch, add or remove patches, you'll still get a correct, incremental build.

Your dependencies can't patch. If you ask for a given commit, you get exactly that commit.

## System dependencies

A *system dependency* is a mirage. It's possible to build against system dependencies reliably on *some* system, but to encompass the quirks and patches of every distro (just to speak of Linux) is a fool's errand.

But you didn't come here for a lecture! Sometimes, you just need to link to something on your damn system.

When people talk about *system dependencies*, they mean two separate things:
1. Linking to a shared library which is resolved at runtime. This is totally fine.
2. Compiling against whatever headers happen to be lying around. This is bad.

The first case is sometimes not just acceptable, but *principled*, regardless of whether you want to. You might link to the system `libcurl.so` to pull in security patches, for example. Or to benefit from bugfixes long after you abandon your program to pursue a career in professional bass fishing. Therefore, you can specify `system_deps` in your manifest:

```toml
[package]
name = "foo"
system_deps = [
  { lib = "pthread", when = { os = "linux" } },
  { lib = "shell32", when = { os = "windows" } }
]

[[lib]]
name = "bar"
system_deps = ["m"]
```

This does what you'd expect, e.g. `-lpthread`. Linking to whatever's on the system comes with plenty of problems, but it's an important tool in your toolbelt.

The second case, however, is almost always bad. Linking to the system makes you unpredictable at *runtime*, but compiling against the system makes you unpredictable at *compile time*. An object file compiled against slightly different versions of `curl.h` are completely different. Your build cache dies. And, at best, you compiled against a header which is ABI compatible with the `libcurl.so` on someone else's machine.

There I go lecturing again! I don't mean to tell anyone that the way they work is wrong. Not everyone needs truly hermetic builds. All I mean to say is that after all the the insane distro wrangling and ABI mess that comes with discovering system headers, `spn` would *still* produce a binary that's fundamentally broken more often than not. If your constraints tolerate this, you can always do this:

```toml
[package]
name = "foo"
include = ["/usr/include"]
```

## The lockfile

