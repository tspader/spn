# overview
`spn` is a package manager and build tool for C projects in the style of UV or cargo

- projects are defined in TOML manifests (e.g. `./spn.toml`)
- build scripts are written in plain C and jitted with embedded TCC (e.g. `./spn.c`)
- `spn` is written in C using `sp.h` as its custom standard library
  - we adhere strictly to its coding style, `./doc/skill/sp.md`
  - reference the `sp` skill or `./doc/sp/SKILL.md` judiciously
- we use a tiny game loop library from `sp.h`, so `sp_main` is the entry point

# building
- `make` to build `bootstrap/bin/spn`
- `./bootstrap/bin/spn build --target $test --profile debug --force`
    - e.g. `./bootstrap/bin/spn build -t integration -p debug -f`
- `./build/debug/store/bin/$test`
    - e.g. `./build/debug/store/bin/integration`

# references
- `source/`
  - `spn.c` is most of the code (large file; search, don't read)
    - `spn_app_t` is all data for one invocation
    - `spn_init` is the top level init function
    - `spn_app_load` loads the package we're operating on
  - `graph.h` is the build DAG
  - `cli.h` is a generic CLI library
- `include/`
  - `spn/spn.h` for public API used in downstream packages
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
assistant: Invokes sp skill
assistant: Reads reference at the beginning of sp.h
assistant: Finds needed code, searches through our code for existing in-context examples
</example>
```

## Rules
- always use the `sp.h` skill when writing against sp.h APIs (either with your `Skill` tool or with `./doc/skill/sp.md`)
- always use SP_ZERO_INITIALIZE() instead of leaving variables uninitialized
- always use braces for one liner scopes (e.g. `for`, `if`)
- always use `foo()` instead of `foo(void)` for no-argument functions
- prefer designated initializers to memberwise assignment
- prefer to use a specific allocator instead of the general purpose global allocator:
  - a memory arena (several in the codebase)
  - string interner (on global spn_ctx_t)
  - scratch arena, if transient (see: sp.h docs)
- always use `sp_for(it, 5)` instead of `for (u32 it = 0; it < 5; it++)`
- always use `it` as your iterator variable (not e.g. `i`)
- prefer `const c8*` for structs or functions that are mostly used with literals, even if you convert to sp_str_t immediately when using
- never re-zero memory returned by sp_alloc()
- never use the C standard library. always use `sp.h`
