# overview
`spn` is a package manager and build tool for C projects in the style of UV or cargo. Key features:
- Projects are defined in TOML manifests (e.g. `./spn.toml`)
- Build scripts are written in plain C and compiled to WASM
- `spn` is written in C using `sp.h` as its custom standard library

# building
We keep two builds; a CMake-based build for building from scratch, and a self hosted build. The CMake build is wrapped with a tiny Makefile, just for coordination rather than build logic:

```
make
```

The self hosted build is a normal `spn` invocation:

```
spn build
```

# references
- `source/`
  - `cli/` and `tui/` are the CLI: a consumer of the spn library like any other. `cli/main.c` is the entry point. Their `.c` files only include `spn/host.h` for library functions; their `types.h` headers may include library `types.h` headers directly, per the types.h rule below.
  - `op/` is the library operations: the build pipeline (resolve, sync, configure, build), plus action verbs (add, clean, publish, index sync, run)
    - `build/` is all the code that sets up and runs inside the build graph
- `include/`
  - `spn/core.h` is the shared vocabulary (enums, `spn_triple_t`, `spn_err_t`) included by everything: guest scripts, host consumers, and library internals. Internal code includes `spn/core.h`, never `spn.h`.
  - `spn.h` is the guest API included by build scripts in downstream packages. Inside the library, only the guest ABI implementation (`source/api/`, `source/external/wasm/abi.h`, `wasm.c`, `codegen/gen/abi.gen.h`) may include it.
  - `spn/host.h` is the single public header for host consumers embedding spn as a library. It is self-contained: it includes only `sp.h` and `spn/core.h`, never internal headers, and its signatures use only public types, opaque handles (`spn_ctx_t`, `spn_session_t`, `spn_target_t`), and request structs it defines itself. Handles are pure forward declarations; the library casts them to internal types at the boundary (`source/host/host.c`, the op verbs), the same way the guest ABI puns `spn_t` to `spn_pkg_unit_t`. If the CLI needs a library capability expressible in public types, that is an API gap to fix in `spn/host.h`, never a reason to widen its includes.
  - Two transitional exceptions are sanctioned until their publicization lands: the event/error protocol (`spn_build_event_t`, `spn_err_union_t`, their renderers) is not yet public, and the CLI still reaches the `spn` ctx global plus `spn_session_config_t`/`spn_profile_info_t` construction through internal types.h includes (`cli/cli.h`). These are gaps being closed by the ctx split and session descriptor work, not the pattern.
  - `spn.h` and `spn/host.h` cannot be included in the same translation unit (both enforce this with `#error`): each declares its own opaque `spn_target_t`, so guest-side and host-side code must stay in separate TUs.
- `spn.toml` is the package for spn itself; it's example of how a real downstream project would use spn
- `test/integration/fixtures/` contains small, hermetic spn projects used in integration tests.
  - `script/build_script` is an excellent example

# tests

@assets/llm/test.md

# compilation
we used to build as a single C file; now, we split into very granular TUs for testing. some code has not been migrated. in general:
- `$module/types.h` is ONLY types, and can be included liberally
- any other header in `$module/` is for functions.
  - if a TU includes such a header, you must either link to its impl in a unit test or mock it
  - you CANNOT include these headers in a `types.h`

if you find that you are mocking anything more than a very small, focused set of modules, you are doing it wrong. break up modules more granularly so that you can JUST mock the few functions you need and link to real impls of others.

if you find that you are pulling in tons and tons of unrelated TUs for a unit test, break up the modules causing all the transitive dependencies!

# Errors

There are two error types:
- `spn_err_t`: a plain error code. Prefer it. Leaf modules return codes; the caller has the context.
- `spn_err_union_t`: a code plus structured payload, for errors whose rendered message needs data. Add a kind to `spn_err_t` in `include/spn/core.h`, a payload struct to `spn_err_union_t` in `source/error/types.h`, and a message in `spn_tui_render_event_detail`.

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

Reporting is the event stream. `spn_err_emit(ctx, err)` is the single gate between the two types: it pushes the error as an `SPN_EVENT_ERR` event exactly once (skipped when `err.reported` is set), records the first error kind on the context for the exit code, and returns the plain code. Frontends render events; they never build error messages themselves.

The rules:
- Library internals construct unions and propagate them unreported with `spn_try_union()`.
- Ops (`spn_op_*`) and context lifecycle (`spn_ctx_*`) return `spn_err_t`. Every union crosses `spn_err_emit()` exactly once at that boundary, so a nonzero code in hand always means "already reported"; frontends only map it to an exit status.
- Concurrent and streaming code (build workers, sync jobs) reports at the point of occurrence -- `spn_err_emit()`, or a specialized error event like `SPN_EVENT_LINK_FAILED` -- and propagates `spn_err_reported(kind)` so the boundary skips it.
- The reporting layer (`tui.c`, `spn_tui_render_event_detail`) is the only place messages are built:

```c
case SPN_ERR_TOOLCHAIN_UNKNOWN: {
  sp_fmt_io(&w.base, "toolchain {.cyan} isn't defined", SP_FMT_STR(event->err.toolchain.name));
  break;
}
```

The complete toolkit is five names; there are deliberately no others:
- `spn_try(expr)` propagates a code
- `spn_try_union(expr)` propagates a union
- `spn_result(kind)` lifts a code into a union
- `spn_err_reported(kind)` lifts an already-reported code into a union
- `spn_err_emit(ctx, err)` reports a union and returns its code

Prefer the try macros unless a conditional reads more simply; report-and-propagate composes as `spn_try(spn_err_emit(ctx, expr))`.

## Rules
- Never, ever comment your code. Code with newly added comments will be rejected. If you're reviewing code, flag comments.
- Always use unprefixed names for private, static functions (e.g. `is_file_clean()` instead of `spn_is_file_clean()`)
- Always use `sp_cstr_as_str()` instead of `sp_str_view()`
- always use sp_zero instead of leaving variables uninitialized
- always use braces for one liner scopes (e.g. `for`, `if`)
- always use `foo()` instead of `foo(void)` for no-argument functions
- always use `sp_for(it, 5)` instead of `for (u32 it = 0; it < 5; it++)`
- always use `it` as your iterator variable (not e.g. `i`)
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

