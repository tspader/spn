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
  - spn/err.h -> (host, public), (host, private), (guest); generated from source/core/codegen/schema/errors.jtd.json, included by spn/core.h
  - spn/errors.h -> (host, public); generated from source/core/codegen/schema/errors.jtd.json
  - spn/events.h -> (host, public); generated from source/core/codegen/schema/events.jtd.json
  - spn.h -> (guest)
- `source/`
  - `source/core/` is the library: every module, with `source/core` as the include root (so internal includes stay `ctx/ctx.h`, `event/event.h`, ...).
  - `source/cli/` is the CLI: a consumer of the spn library like any other, with `source/cli` as its include root (all includes are root-relative; no relative includes). `main.c` parses and dispatches; `tui/` renders the event stream. `commands/` holds one file per command, each owning its `sp_cli_cmd_t` node, its handler (static), and its parsed-argument storage (file-static) -- `commands/commands.h` exports only the tree (`spn_cli()`). `commands/util/util.{h,c}` is the command runtime: the host state (`host`, `tui`), boot/shutdown, and the open/session/op/wait plumbing every command uses; its audience is the command files and `main.c`. CLI files include only `spn/host.h` for library functionality.
  - In `source/core/`:
    - `op/` is the op layer: the worker thread and queue (`op.c`), plus the op bodies (build, add, clean, publish, index sync, run, scaffold)
    - `model/` establishes the session's model: the resolve/sync/configure stages plus the driver that runs them to a fixpoint. `model.h` declares its one entry point (`spn_model_establish`); `stage.h` is module-internal
    - `graph/` is all the code that sets up and runs inside the build graph; it serves both `model/` (the configure graph) and the build op (the target graph)
- `include/`
  - `spn/core.h` is the sp-free shared vocabulary (enums, `spn_triple_t`, `spn_err_t`) included by everything: guest scripts, host consumers, and library internals. Internal code includes `spn/core.h`, never `spn.h`. The `spn_err_t` enum is generated from the error schema into `spn/err.h`, which `spn/core.h` includes; both are sp-free and both ship in the guest sysroot.
  - `spn/types.h` is the sp-dependent shared vocabulary: the value types that describe work and results (`spn_session_config_t`, target selection, profile overrides, op requests and results, index descriptors). It declares no handles, no functions, and carries no `#error` guard, so it is safe in any TU; internal `types.h` headers include it directly, and `spn/host.h` re-exports it. If a type has identity (a handle) or behavior (a function), it is surface, not vocabulary, and does not belong here.
  - `spn.h` is the guest API included by build scripts in downstream packages. Inside the library, only the guest boundary (`source/core/api/`, `source/core/external/wasm/abi.h`, `wasm.c`, `guest.c`, `codegen/gen/abi.gen.*`) may include it; those TUs also include internal headers, and `spn.h` gives the compiler the public prototypes to check the implementations against.
  - `spn/host.h` is the host embedding API: opaque handles plus functions, nothing else. Every handle aliases the real internal tag (`spn_ctx_t`, `spn_session_t`, `spn_op_t`, and `spn_target_t` is `struct spn_target_unit`), so host-boundary TUs (`source/core/host/host.c`, the op verbs, `ctx/`, `triple/`, `enum/`) see the complete type and use handles with no casts, while embedders see pure forward declarations. If the CLI needs a library capability expressible in public types, that is an API gap to fix in `spn/host.h`, never a reason to widen its includes.
  - Tag ownership: typedef names belong to a world -- guest `spn_target_t` and host `spn_target_t` are different types on purpose -- and a struct tag belongs to whoever defines it. Guest handle tags (`struct spn`, `struct spn_config`, `struct spn_target`, `struct spn_profile`) are defined nowhere; the guest boundary puns them to internal types (`spn_t` to `spn_pkg_unit_t`, `spn_target_t` to `spn_target_info_t`). `struct spn_node` is the one guest handle with its own state; `source/core/api/types.h` defines it. Internal headers never mention guest names.
  - `spn/errors.h` and `spn/events.h` are the event/error protocol: generated from the schemas in `source/core/codegen/schema/`, containing only `sp.h` and `spn/core.h` types. They are public because `spn_ctx_drain` returns `spn_event_t*`; `spn/host.h` includes `spn/events.h` so a host sees the full protocol from one include. The generator writes them to `include/spn/` directly (never hand-edit them) and rejects any schema that would pull a private header into them.
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
- `spn_err_t`: a plain code. It is the only error type that crosses function boundaries; every fallible function returns it.
- `spn_err_union_t`: a code plus structured payload, for errors whose rendered message needs data. The schema is the single source of truth: add a mapping entry to `source/core/codegen/schema/errors.jtd.json` and a message in `spn_tui_render_event_detail`; `make` regenerates the `spn_err_t` enum (`include/spn/err.h`), the payloads (`include/spn/errors.h`), and the writers (`source/core/codegen/gen/errors.gen.c`).

Reporting is the event stream. A failure is detected exactly once, at the site with the context; that site constructs the union literal and reports it:

```c
return spn_err_emit(ctx, (spn_err_union_t) {
  .kind = SPN_ERR_FS_WRITE,
  .fs = { .path = output },
});
```

`spn_err_emit(ctx, err)` pushes the union as an `SPN_EVENT_ERR` event, records the first error kind on the context for the exit code, and returns the plain code. Callers above the detection site just `spn_try` the code. Frontends render events; they never build error messages themselves.

The rules:
- Domain failure events (`SPN_EVENT_LINK_FAILED`, `SPN_EVENT_TARGET_BUILD_FAILED`, `SPN_EVENT_TEST_FAILED`, `SPN_EVENT_SYNC_FAILED`, `SPN_EVENT_NODE_FAILED`, the script compile and crash events) are already the diagnostic for their failures; those paths return the bare code and never emit an err event. Cancellation (`SPN_ERR_CANCELLED`) is never emitted either.
- Layers below the event stream return codes plus structured out-params and never emit: `source/core/dag/` (`spn_dag_diag_t` + trace callbacks), the toc parser, the git cache, `spn_pkg_load` (`spn_codegen_issues_t`), the index getters (`spn_index_diag_t`), sp-level io. The spn-layer caller is the detection site: `graph/dag.c` `dag_result()` converts `env.diag` into the union and emits it, while its `SPN_ERR_DAG_CANCELLED`/`SPN_ERR_DAG_ACTION` arms return the bare code because their diagnostics already went out as node events.
- Speculative code emits at its commit point, not at detection. The resolver backtracks, so `source/core/resolve/` internals pass `spn_err_union_t` between themselves and accumulate survivors in `query->errors`; `model/resolve.c` flushes each through `spn_err_emit` and returns the code. The union appears in no signature outside those internals except as `spn_err_emit`'s parameter.
- Ops are data: verbs return an `spn_op_t*` handle; completion is state (`spn_op_done()`, `spn_op_result()`), signaled through the host's wake callback (registered at `spn_ctx_new`), and cancellation is per-op (`spn_op_cancel()`). The op thread records the result code on the context after exec, so the `err` in `spn_op_result()` always means "already reported"; frontends only map it to an exit status. Context lifecycle (`spn_ctx_*`) and public queries return `spn_err_t` under the same contract.
- The reporting layer (`tui.c`, `spn_tui_render_event_detail`) is the only place messages are built; its `SPN_EVENT_ERR` arm is a single flat switch on `event->err.kind`:

```c
case SPN_ERR_TOOLCHAIN_UNKNOWN: {
  sp_fmt_io(&w.base, "toolchain {.cyan} isn't defined", SP_FMT_STR(event->err.toolchain.name));
  break;
}
```

The complete toolkit is two names; there are deliberately no others:
- `spn_try(expr)` propagates a code
- `spn_err_emit(ctx, (spn_err_union_t) { ... })` reports at the detection site and returns the code

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

