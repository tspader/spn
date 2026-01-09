# overview
`spn` is a package manager and build tool for C projects in the style of UV or cargo

- projects are defined in TOML manifests (e.g. `./spn.toml`)
- build scripts are written in plain C and jitted with embedded TCC (e.g. `./spn.c`)
- `spn` is written in C using `sp.h` as its custom standard library
  - we adhere strictly to its coding style, `./doc/skill/sp.md`
  - reference the `sp` skill or `./doc/sp/SKILL.md` judiciously

# building
- `spn` is self-hosting
  - `pspn` is a stable build i have copied on the $PATH; use it to build `spn`
  - `tspn` is a link on the $PATH to the debug binary; use it when testing what you built
- always build with `--profile debug` unless explicitly testing another profile
- `build/debug` is the top level build output dir
  - `build/debug/work` is for intermediate outputs and logs
  - `build/debug/store` is for final outputs and binaries
- example workflow:
  1. make code changes
  2. `pspn build --profile debug` to build with known-good copy
  3. `tspn build -p debug -f` to use the new binary to test by force building

# references
- `source/spn.c` is most of the code (large file; search, don't read)
- `source/graph.h` is the build DAG
- `source/cli.h` is a generic CLI library
- `include/spn/spn.h` for public API used in downstream packages
- `spn.toml` is the package for spn itself; (example of a downstream project that uses spn)
- `examples/tcc/spn.toml` is the package for the `tcc` example (minimal example of a downstream project)
- `packages/sp/spn.toml` is the package for `sp.h` (example of a source-only spn package)
- `packages/tcc/spn.toml` is the package for `tcc` (example of a compiled spn package)

# tests
- `test/core` is unit tests; build and run with `pspn test` or `pspn test --target $target`
- `test/manual` is manual tests; each test has instructions in `test.md` and a clean project
- use ./build/llm as your temporary directory when testing; do not use /tmp

# examples
## finding code in sp.h
```xml
<example>
user: Write a function that reads a file and logs its contents
assistant: [Uses Task tool and sp skill to find relevant APIs; looks through spn.c for existing contextual examples]
</example>
```

## Rules
- always use the `sp.h` skill when writing against sp.h APIs (either with your `Skill` tool or with `./doc/skill/sp.md`)
- never use the C standard library. always use `sp.h`
- always prefer to initialize structs with designated initializers when possible
  - you MUST use SP_ZERO_INITIALIZE() if you don't use a designated initializer
  - sp_alloc() and SP_ALLOC() return zero allocated memory; do not re-zero
- always use braces for one liner scopes (e.g. `for`, `if`)
- allocations are never done through the generic global allocator; prefer to allocate from:
  - a memory arena (several in the codebase)
  - string interner (on global spn_ctx_t)
  - scratch arena, if transient (see: sp.h docs)
