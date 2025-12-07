# CLI Refactor Plan

## Current State

**The problem:** CLI handling is split across:
1. `spn_init()` (lines 5601-6073) - global options parsing + command schema definitions (huge inline `spn_cli_usage_t` struct)
2. Individual `spn_cli_*` functions - each re-parses with its own local schema definition

**Key issues:**
1. **Duplicate schemas** - Commands defined twice: once in `spn_init()`'s `spn_cli_usage` (for help), once inline in each `spn_cli_*` function (for actual parsing)
2. **Inconsistent patterns** - `spn_cli_build()` re-parses (line 7087-7092), but schema is already in `spn_init()` 
3. **Schema/handler decoupling** - Schema at lines 5635-5892, dispatch at 6056-6070, handlers at 6577+
4. **Subcommand handling** - Complex inline subcommand parsing in `spn_init()` (lines 5912-5961) for `tool`, but subcommand structs defined separately
5. **Mixed global parsing** - Global opts parsed early (5612-5629), then command-specific opts parsed later in handlers

---

## Minimal Plan

1. **Single source of truth for schemas**
   - Move all `spn_cli_command_usage_t` definitions to a static table (e.g., `spn_cli_commands[]`)
   - Include the handler fn pointer directly in schema: `void (*handler)(spn_cli_t*)`
   
2. **Remove duplicate parsing**  
   - `spn_init()` parses global opts only, then looks up command and calls `spn_cli_dispatch(cmd_schema)`
   - `spn_cli_dispatch()` parses command opts/args using the schema's `.ptr` bindings, then calls `schema->handler(cli)`
   - Individual `spn_cli_*` functions become pure handlers (no parsing)

3. **Unify subcommand handling**
   - `spn_cli_subcommand_usage_t` already exists; just wire it the same way
   - `spn_cli_tool()` dispatches to `spn_cli_tool_install()`, etc. without re-parsing

4. **Result:** One ~100-line dispatch function replaces 500+ lines of scattered parsing
