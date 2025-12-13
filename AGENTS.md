# Project Overview
- **spn**: Package manager and build tool for C projects in the style of UV or cargo (spn.toml + spn.c)
- written in modern C using `sp.h` (use the `sp` skill or `./doc/sp/SKILL.md` if you do not have access to global skills)
  - `spn` is an extension of `sp.h`, so its instructions and style are exactly the same as this project's
  - reference the `sp` skill or `./doc/sp/SKILL.md` judiciously
```xml
<example>
user: Write a function that reads a file and logs its contents
assistant: [Uses Task tool and sp skill to find relevant APIs; looks through spn.c for existing contextual examples]
</example>
```

# Files
- `source/spn.c` is the entire implementation
- `include/spn/spn.h` for public API used in packages
- `spn.toml` is the package for spn itself; (example of a downstream project that uses spn)
- `examples/tcc/spn.toml` is the package for the `tcc` example (minimal example of a downstream project)
- `packages/sp/spn.toml` is the package for `sp.h` (example of a source-only spn package)
- `packages/tcc/spn.toml` is the package for `tcc` (example of a compiled spn package)

# Build
- build with `spn build --profile=debug` to output to `./build/debug`
- if you need to test a change you made, building spn itself is often the best way; use e.g. `./build/debug/spn build --profile=debug` to use a fresh debug build
- if you, for some reason, need a non-bootstrapped build, build with `make` to output to `./bootstrap/bin/spn`

## Rules
- always use the `sp.h` skill when writing against sp.h APIs (either with your `Skill` tool or with `./doc/llm/sp/SKILL.md`)
- never use the C standard library. always use `sp.h`
- always prefer to initialize structs with designated initializers when possible
  - you MUST use SP_ZERO_INITIALIZE() if you don't use a designated initializer
  - sp_alloc() and SP_ALLOC() return zero allocated memory; do not re-zero
