---
title: Dependencies
order: 4
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

What if you need to patch `foo`? You don't want to go back to the technology of the ancients, like submodules or vendoring. You want a specific commit with just a few changes. Do this in your `spn.toml`:

```toml
[deps.package]
foo = "6.9.0"

[patch.foo]
files = ["patches/foo.patch"]
```

That's it! Patches fold into the build system like everything else. If you change the contents of a patch, add or remove patches, you'll still get a correct, incremental build.

Your dependencies can't patch. If you ask for a given commit, you get exactly that commit.

## The lockfile
