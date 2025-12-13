# spn generate - Implementation Plan

## Summary

The `spn_cli_print` function (lines 7113-7221) is commented out and returns `SPN_CLI_UNIMPLEMENTED()`. Its purpose was to export dependency information (include paths, library paths, libs, rpaths, system deps) in formats suitable for external build systems (raw, shell script, Makefile).

## Problems

1. **Function is disabled**: `spn_cli_print` at line 7113 immediately returns `SPN_CLI_UNIMPLEMENTED()`, all logic is commented out
2. **CLI registration exists but is dead**: CLI entry at line 6128-6158 still registers "print" command with options `--generator`, `--compiler`, `--path`, but handler does nothing
3. **Missing resolution step**: Commented code shows correct pattern (`spn_app_resolve()` + `spn_app_prepare_build()`) but these calls are commented out (lines 7130-7131)
4. **Rename needed**: User wants command renamed from "print" to "generate"

## Key References

| Symbol | Location | Purpose |
|--------|----------|---------|
| `spn_cli_print` | spn.c:7113 | Handler function (currently disabled) |
| `spn_cli_print_t` | spn.c:1261-1264 | CLI args struct: generator, compiler, path |
| CLI registration | spn.c:6128-6158 | Command definition with opts |
| `spn_app_resolve` | spn.c:4784 | Resolve deps from lock file or solver |
| `spn_app_prepare_build` | spn.c:5201 | Create `spn_pkg_ctx_t` for each dep with build_id and paths |
| `spn_gen_build_entries_for_all` | spn.c:2554 | Get formatted compiler flags for all deps |
| `spn_gen_build_entry_for_dep` | spn.c:2467 | Get entries for single dep by kind |
| `spn_gen_format_entry_for_compiler` | spn.c:2402 | Format entry with compiler-specific prefix (-I, -L, etc.) |
| `spn_generator_kind_t` | spn.c:429-433 | Enum: RAW, SHELL, MAKE |
| `spn_gen_entry_kind_t` | spn.c:435-444 | Enum: INCLUDE, LIB_INCLUDE, LIBS, SYSTEM_LIBS, RPATH, DEFINE |
| `app.build.contexts.deps` | spn.c:838 | Hash table of `spn_pkg_ctx_t` keyed by name |
| `app.resolver.system_deps` | spn.c:925 | System library dependencies |

## Plan

1. **Rename CLI command**: Change `"print"` → `"generate"` at line 6129; rename `spn_cli_print_t` → `spn_cli_generate_t`; rename `spn_cli_print` → `spn_cli_generate`; update all references

2. **Uncomment resolution calls**: Restore lines 7130-7131:
   ```c
   spn_app_resolve(&app);
   spn_app_prepare_build(&app);
   ```

3. **Require lock file**: Add check that `app.lock.some == SP_OPT_SOME`, fatal error otherwise (deps must already be built)

4. **Restore generator context init**: Uncomment lines 7133-7147 to build `spn_generator_context_t` with include/lib_include/libs/rpath/system_libs

5. **Restore output generation**: Uncomment lines 7149-7219 - switch on `gen.kind`:
   - `SPN_GEN_KIND_RAW`: concatenate all flags
   - `SPN_GEN_KIND_SHELL`: output `export SPN_*=...` 
   - `SPN_GEN_KIND_MAKE`: output `SPN_* := ...`

6. **Restore file output**: If `--path` specified, write to `path/spn.sh` or `path/spn.mk`; else print to stdout
