# overview
`spn` is a package manager and build tool for C projects in the style of UV or cargo

- projects are defined in TOML manifests (e.g. `./spn.toml`)
- build scripts are written in plain C and jitted with embedded TCC (e.g. `./spn.c`)
- `spn` is written in C using `sp.h` as its custom standard library
  - we adhere strictly to its coding style, `./doc/skill/sp.md`
  - reference the `sp` skill or `./doc/sp/SKILL.md` judiciously
- we use a tiny game loop library from `sp.h`, so `sp_main` is the entry point

# building
- `cmake -S . -B bootstrap/work` (once) and `cmake --build bootstrap/work` to build `bootstrap/bin/spn` + tests
- `./bootstrap/bin/spn build --target $test --profile debug --force`
    - e.g. `./bootstrap/bin/spn build -t integration -p debug -f`
- `./build/debug/store/bin/$test`
    - e.g. `./build/debug/store/bin/integration`
- aliases:
    - `bspn` -> `./bootstrap/bin/spn`
    - `dspn` -> `./build/debug/store/bin/spn`
- the Full Process for testing changes:
    - `make` (build a clean bootstrap binary)
    - `bspn build -t spn -p debug -f` (build spn using itself)
    - `dspn build -t integration -p debug -f` (build the tests with bootstrapped spn)
    - `./build/debug/store/bin/integration` (run tests)

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
- use ./.llm/tmp as your temporary directory when testing; do not use /tmp

our tests are a virtual machine; we do NOT write imperative tests. tests are data (bytecode) which get fed through the same executor. this ensures consistent setup and expectation, and lets us write many tests without breaking down. you may write new executors when a category of tests differs from existing categories; but you may NOT write imperative logic inside a test, and you may NOT make an "executor" for a single test

all tests pass all the time. if a test fails but is unrelated to your code change, you fix it. there are no flaky tests; it is ALWAYS your responsibility to fix a broken test.

# compilation
we used to build as a single C file; now, we split into very granular TUs for testing. some code has not been migrated. in general:
- `$module/types.h` is ONLY types, and can be included liberally
- any other header in `$module/` is for functions.
  - if a TU includes such a header, you must either link to its impl in a unit test or mock it
  - you CANNOT include these headers in a `types.h`

if you find that you are mocking anything more than a very small, focused set of modules, you are doing it wrong. break up modules more granularly so that you can JUST mock the few functions you need and link to real impls of others.

if you find that you are pulling in tons and tons of unrelated TUs for a unit test, break up the modules causing all the transitive dependencies!

# error handling
we return errors up the call stack in an error union (by which we mean error code + rich error data, not a union of error code and function result). it's OK to return just the status when the rich data isn't needed.

## try macros
try macros are very important, to keep the code concise. prefer them, but it's OK to use regular conditional statements rather than a mess of macros. rule of thumb: if the error-producing code is a single function call, use a try macro

### sp_try()
sp_try() returns an error code if nonzero
```c
spn_err_t err = fn();
if (err != SPN_OK) {
  return err;
}

sp_try(fn());
```

### spn_try_as()
spn_try_as() returns a foreign error code as SPN_ERROR if nonzero
```c
foo_err_t err = library_fn_that_doesnt_return_spn_error_t();
if (err != FOO_OK) {
  return SPN_ERROR;
}

spn_try_as(fn());
```

### spn_try_union()
spn_try_union returns an error union if nonzero
```c
spn_err_union_t err = fn();
if (err.kind != SPN_OK) {
  return err;
}

spn_try_union(fn());
```

### spn_try_union()
spn_try_as_union() returns an error code as a union if nonzero
```c
spn_err_t err = fn();
if (err != SPN_OK) {
  return (spn_err_union_t) { .kind = err };
}

spn_try_as_union(fn());
```

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
- always use `sp_for(it, 5)` instead of `for (u32 it = 0; it < 5; it++)`
- always use `it` as your iterator variable (not e.g. `i`)
- always use the following names in tests:
  - `test` for the name of a project
  - `main` for the name of a binary
  - `spum` for the name of a consumed package
  - `SPUM` when verifying a preprocessor define from a header in a package; `69` when a constant is needed
- always use the `sp_da` alias of `sp_dyn_array`
- always use `lower_snake` macros, regardless of surrounding code
- always use `spn_err_t` as the return type of a function which can fail; errors are returned up the call stack, go-style
  - prefer the `sp_try(expr)` family of macros for a zig-style try instead of `err = expr; if (err) { return err; }`
- prefer `const c8*` for structs or functions that are mostly used with literals, even if you convert to sp_str_t immediately when using
- prefer designated initializers to memberwise assignment
- prefer to use a specific allocator instead of the general purpose global allocator:
  - a memory arena (several in the codebase)
  - string interner (on global spn_ctx_t)
  - scratch arena, if transient (see: sp.h docs)
- prefer to use inner structs rather then lexical prefix (e.g. `foo.bar`, `foo.baz` instead of `foo_bar` and `foo_baz`)
- prefer `sp_io_write_new_line()` or `sp_io_write_line()` to `sp_io_write_str("\n")`
- prefer one word names for directories and file names
- never re-zero memory returned by sp_alloc()
- never use the C standard library. always use `sp.h`
- never ever use SP_UNUSED() or mark unused arguments in any way. just ignore them.
- never allocate and free individual allocations; every allocation should be tied to some kind of bulk allocation (e.g. an arena) and freed alongside
- never comment your code, under any circumstances
  - a single explanatory comment per test case is permissible if such comments exist on other tests in file
- never name out parameters "out"; name them as you normally would


