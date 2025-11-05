# Project Overview
- **spn**: Package manager and build tool for C projects in the style of UV or cargo (spn.toml + spn.c)
- Written in C using **sp.h** (single-header standard library replacement)
- Core files: `source/spn.c` for all implementation code, `include/spn/spn.h` for public API
- **ALWAYS activate the `sp` skill** when working with this codebase: use `Skill("sp")`
- No memory leak concerns - this codebase doesn't free memory
- build with plain `make`, don't run it or write unit tests
