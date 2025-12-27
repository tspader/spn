---
name: spn
description: Guide for spn, a package manager and build tool for C. Use when creating spn projects, managing dependencies, building, testing, or integrating with external build systems.
---

# spn

Package manager and build tool for C projects using sp.h.

## Quick Reference

```bash
spn init              # new project
spn add <pkg>         # add dependency
spn build             # build project
spn test              # run tests
spn clean             # clear cache
```

## Project Setup

### Initialize
```bash
spn init                 # creates spn.toml + spn.c + dependency on sp.h
spn init --bare          # minimal: just spn.toml
```

### Manifest (spn.toml)
```toml
[package]
name = "myproject"
version = "0.1.0"

[deps.package]
sp = "1.6.6"
somelib = ">=1.0.0"

[deps.test]
utest = "1.0.0"

[[bin]]
name = "myproject"
source = ["source/main.c"]
include = ["include"]

[[test]]
name = "mytest"
source = ["test/main.c"]
include = ["include", "source"]

[[profile]]
name = "debug"
cc = "clang"
```

## Dependencies

```bash
spn add <package>           # add latest version
spn add <package> --test    # add as test dependency
spn update <package>        # update to latest
spn list                    # list all available packages
```

## Building

```bash
spn build                       # build project
spn build --force               # rebuild everything
spn build --profile debug       # use named profile
spn build --target mybin        # build specific target
spn build --tests               # include test targets
```

## Testing

```bash
spn test                        # build and run all tests
spn test --target mytest        # run specific test
spn test --profile debug        # with profile
```

## Cache Inspection

```bash
spn which                       # show cache root
spn which <package>             # show package cache path
spn which --dir store           # show store directory
spn which --dir lib <package>   # show lib dir for package

spn ls                          # list cache contents
spn ls <package>                # list package contents
spn ls --dir lib <package>      # list package libs

spn clean                       # remove cache and store
```

Directory kinds: `store`, `include`, `lib`, `source`, `work`, `vendor`

## Generate (Escape Hatch)

Export dependency flags for external build systems:

```bash
spn generate                              # print raw flags
spn generate --generator make --path .    # create spn.mk
spn generate --generator shell --path .   # create spn.sh
spn generate --generator cmake --path .   # create spn.cmake
```

Options: `--generator` (raw, shell, make, cmake), `--compiler` (gcc, clang, tcc), `--path`

Generated files export: `SPN_INCLUDES`, `SPN_LIB_INCLUDES`, `SPN_LIBS`, `SPN_RPATH`, `SPN_SYSTEM_LIBS`, `SPN_FLAGS`

## Tools

```bash
spn tool install <package>              # install binary to PATH
spn tool install <package> --version X  # specific version
spn tool uninstall <package>            # remove from PATH
spn tool run <package> [cmd]            # run package binary
```

## Other Commands

```bash
spn manifest <package>          # print package manifest
spn graph                       # output mermaid build graph
spn graph --output graph.md     # save to file
```
