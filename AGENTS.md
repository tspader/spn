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
- include/
  - spn/host.h -> (host, public)
  - spn/types.h -> (host, public), (host, private)
  - spn/core.h - (host, public), (host, private), (guest)
  - spn.h -> (guest)
- source/
  - cli/
- `source/`
  - `cli/` and `tui/` are the CLI: a consumer of the spn library like any other. `cli/main.c` is the entry point. `cli/*.c` files only include `spn/host.h` for library functions; `tui/` additionally renders the internal event protocol, per the transitional exception below.
  - `op/` is the library operations: the build pipeline (resolve, sync, configure, build), plus action verbs (add, clean, publish, index sync, run)
    - `build/` is all the code that sets up and runs inside the build graph
- `include/`
  - `spn/core.h` is the sp-free shared vocabulary (enums, `spn_triple_t`, `spn_err_t`) included by everything: guest scripts, host consumers, and library internals. Internal code includes `spn/core.h`, never `spn.h`.
  - `spn/types.h` is the sp-dependent shared vocabulary: the value types that describe work and results (`spn_session_config_t`, target selection, profile overrides, op requests and results, index descriptors). It declares no handles, no functions, and carries no `#error` guard, so it is safe in any TU; internal `types.h` headers include it directly, and `spn/host.h` re-exports it. If a type has identity (a handle) or behavior (a function), it is surface, not vocabulary, and does not belong here.
  - `spn.h` is the guest API included by build scripts in downstream packages. Inside the library, only the guest boundary (`source/api/`, `source/external/wasm/abi.h`, `wasm.c`, `guest.c`, `codegen/gen/abi.gen.*`) may include it; those TUs also include internal headers, and `spn.h` gives the compiler the public prototypes to check the implementations against.
  - `spn/host.h` is the host embedding API: opaque handles plus functions, nothing else. Every handle aliases the real internal tag (`spn_ctx_t`, `spn_session_t`, `spn_op_t`, and `spn_target_t` is `struct spn_target_unit`), so host-boundary TUs (`source/host/host.c`, the op verbs, `ctx/`, `triple/`, `enum/`, `error/str.c`) see the complete type and use handles with no casts, while embedders see pure forward declarations. If the CLI needs a library capability expressible in public types, that is an API gap to fix in `spn/host.h`, never a reason to widen its includes.
  - Tag ownership: typedef names belong to a world -- guest `spn_target_t` and host `spn_target_t` are different types on purpose -- and a struct tag belongs to whoever defines it. Guest handle tags (`struct spn`, `struct spn_config`, `struct spn_target`, `struct spn_profile`) are defined nowhere; the guest boundary puns them to internal types (`spn_t` to `spn_pkg_unit_t`, `spn_target_t` to `spn_target_info_t`). `struct spn_node` is the one guest handle with its own state; `source/api/types.h` defines it. Internal headers never mention guest names.
  - One transitional exception is sanctioned until its publicization lands: the event/error protocol (`spn_build_event_t`, `spn_err_union_t`, their renderers) is not yet public, so `tui/` — the CLI's renderer of that protocol — includes internal library headers (`event/`, `ctx/`, `enum/`, `semver/`, `toml/`, `toolchain/`) and reaches the `spn` ctx global. This is a gap being closed by the event publicization work, not the pattern. The CLI command layer (`cli/*.c`) has no such exception: it consumes only `spn/host.h` plus its own `cli/` and `tui/` headers.
  - `spn.h` and `spn/host.h` cannot share a translation unit (both enforce this with `#error`). Because every internal header is reachable from both boundaries, this mechanically forbids internal headers from including either surface header: leaking `spn/host.h` into an internal header breaks every guest-boundary TU, and leaking `spn.h` breaks every host-boundary TU.
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
- Ops are data: `spn_op_*` verbs return an `spn_op_t*` handle, and `spn_op_wait()` returns the op's `spn_err_t`. An op's exec function is the boundary: every union crosses `spn_err_emit()` exactly once inside it, so the code `spn_op_wait()` hands back always means "already reported"; frontends only map it to an exit status. Context lifecycle (`spn_ctx_*`) and public queries return `spn_err_t` under the same contract.
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

