# overview
`spn` is a package manager and build tool for C projects in the style of UV or cargo. Key features:
- Projects are defined in TOML manifests (e.g. `./spn.toml`)
- Build scripts are written in plain C and compiled to WASM
- `spn` is written in C using `sp.h` as its custom standard library

# building
The top-level Makefile wraps CMake (fetch pinned deps, configure, build with `zig cc`):
```
make
```

Layout: deps are pinned by SHA in `tools/cmake/fetch.cmake` and checked out into `.build/source`; the CMake work dir is `.build/work/$TRIPLE`; final artifacts go to `.build/store/$TRIPLE/{bin,lib,include,test}`. `bootstrap` is a symlink to the host store, so the binary is at `./bootstrap/bin/spn`.

The bootstrapped binary can then build itself:
```
./bootstrap/bin/spn build -t spn -p debug
```

It is sometimes useful to run the binary thus produced:
```
./build/debug/store/bin/spn build -t spn -p debug
```

# references
- `source/`
  - `spn.c` is the entry point
  - `task/` is the entry points for each phase of work (e.g. resolve, sync, configure, build, etc)
    - `build/` is all the code that sets up and runs inside the build graph
- `include/`
  - `spn/spn.h` for public API used in downstream packages
- `spn.toml` is the package for spn itself; it's example of how a real downstream project would use spn
- `test/fixtures/` contains small, hermetic spn projects used in integration tests.
  - `api/build_script` is an excellent example

# tests

Tests must be written declaratively, by expressing test cases as pure data which are run through a test executor. The executor does setup, execution, expectation, and teardown according to the data in the test case. Imperative logic lives in the executor.
- You can (and should, for larger suites) have multiple executors. Testing a feature does not mean jamming every test into one executor.
- You can drop into imperative logic only when there is a single test which does not conform to the pattern

Tests should be:
- Minimal. Every test should test exactly one feature, and should only have what's needed for that. No fluff.
- Orthogonal. Every feature should be tested in exactly one place.
- High value. We shouldn't burden ourself with brittle tests, or tests that wouldn't catch real bugs.
- Complete. Every feature should be tested.

## running
Running the tests via CTest is easiest, and outputs to an ISO timestamped directory in `.tmp`:
```sh
make test
```
or, equivalently, `ctest --test-dir .build/work/$TRIPLE --output-on-failure`.

## notes

- Use literal friendly types, like `const c8*` and `T [N]` (i.e. fixed size C arrays)
- Use `sp_carr_for()` + zero-as-sentinel (when possible) to avoid typing sentinels or lengths at the test site
- Use a separate struct for `.expect`
- Never explicitly initialize fields which are zero initialized (e.g. do not set `.err = SP_OK`)
- When test cases need multistep, ordered setup, used a tagged union of actions (see: `fs_setup_t`)
- One class of tests per C file. If a suite has multiple, write the individual C files in `test/$module/`, and then have `test/module.c` `#include` all the C files (see: `test/fs.c`)

## example

Follow this structure when adding new tests.

```c
#define FOO_TEST_MAX_BAZ 8

typedef struct {
  bool spum;
  sp_err_t err;
  const c8* kram;
} foo_expect_t;

typedef struct {
  u32 bar;
  const c8* baz [FOO_TEST_MAX_BAZ]
  foo_expect_t expect;
} foo_test_t;

UTEST_EMPTY_FIXTURE(foo)

void run_foo_test(s32* utest_result, foo_test_t t) {
  sp_carr_for(it, t.baz) {
    if (!t.baz[it]) break;
    // ...do something with baz[it]
  }

  EXPECT_TRUE(t.spum);
  // ...verify expectations
}

UTEST_F(foo, large_bar_ok) {
  run_foo_test(&ur, (foo_test_t) {
    .bar = 69,
    .baz = { "skam", "grum", "qux" },
    .expect = {
      .spum = 69
    }
  });
}
```


# compilation
we used to build as a single C file; now, we split into very granular TUs for testing. some code has not been migrated. in general:
- `$module/types.h` is ONLY types, and can be included liberally
- any other header in `$module/` is for functions.
  - if a TU includes such a header, you must either link to its impl in a unit test or mock it
  - you CANNOT include these headers in a `types.h`

if you find that you are mocking anything more than a very small, focused set of modules, you are doing it wrong. break up modules more granularly so that you can JUST mock the few functions you need and link to real impls of others.

if you find that you are pulling in tons and tons of unrelated TUs for a unit test, break up the modules causing all the transitive dependencies!

# Errors

We return errors up the call stack as a return code or an error union, which is just a return code + structured error data. Prefer a plain return code. If your error has associated data (e.g. a name used in its rendered error message), use `spn_err_union_t`.
- Add a kind to `spn_err_t` in `include/spn.h`
- Optionally, dd a payload struct to `spn_err_union_t` in `source/error/types.h`

```c
spn_err_union_t spum(kram_t kram) {
  if (!kram.gaz) {
    return (spn_err_union_t) {
      .kind = SPN_ERR_EXPECTED_KRAM_IN_SPUM,
      .qux = { .slurf = kram.slurf }
    };
  }
  return spn_result(SPN_OK);
}
```

The caller emits the error as an event and propagates the status:

```c
spn_err_union_t err = spn_toolchain_select(&session->catalog, query, session->mem, &session->selection);
if (err.kind) {
  spn_event_buffer_push(session->events, (spn_build_event_t) { .kind = SPN_EVENT_ERR, .err = err });
  return SPN_ERROR;
}
```

The reporting layer (`tui.c`, `spn_tui_render_event`) is the only place messages are built:

```c
case SPN_ERR_TOOLCHAIN_UNKNOWN: {
  sp_fmt_io(&w.base, "toolchain {.cyan} isn't defined", SP_FMT_STR(event->err.toolchain.name));
  break;
}
```

## `try()` macros

We use `try()` macros to keep code concise. There is a family of them (`spn_try()`, `spn_try_as()`, `spn_try_union()`, `spn_try_union_as()`). Prefer them, unless a conditional reads more simply.

```c
spn_try(expr());

// ...expands to
spn_err_t err = fn();
if (err != SPN_OK) {
  return err;
}

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
  - prefer the `spn_try(expr)` family of macros for a zig-style try instead of `err = expr; if (err) { return err; }`
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

