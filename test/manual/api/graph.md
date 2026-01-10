# User-Defined Build Graph Nodes API

## Overview
Added a public API for build scripts to define custom nodes in spn's build dependency graph. Nodes can have file inputs/outputs, explicit ordering dependencies, and custom execution callbacks.

## Public API (`include/spn.h`)

### Types
```c
typedef struct spn_node_t {
  spn_build_ctx_t* ctx;
  u32 index;
} spn_node_t;

typedef struct spn_node_ctx_t {
  spn_build_ctx_t* build;
  void* user_data;
} spn_node_ctx_t;

typedef spn_err_t (*spn_node_fn_t)(spn_node_ctx_t*);
```

### Functions
- `spn_node_t spn_add_node(spn_build_ctx_t* b, const c8* tag)` - Create a node with unique tag
- `void spn_node_set_fn(spn_node_t node, spn_node_fn_t fn)` - Set execution callback
- `void spn_node_set_user_data(spn_node_t node, void* user_data)` - Set callback userdata
- `void spn_node_add_input(spn_node_t node, const c8* input)` - Add file input dependency
- `void spn_node_add_output(spn_node_t node, const c8* output)` - Add file output
- `void spn_node_link(spn_node_t from, spn_node_t to)` - Make `to` depend on `from`
- `void spn_write_file(spn_build_ctx_t* b, const c8* path, const c8* content)` - Write file to work dir

## Implementation (`source/spn.c`)

### Data Structures
- `spn_user_node_t` - Stores user node configuration (tag, callback, inputs/outputs, deps)
- `spn_build_ctx_t::file_to_graph_id` - Per-package hash table for file node deduplication

### Graph Construction
`spn_build_graph_add_user_nodes()` performs multi-pass construction:
1. **Pass 1**: Create command nodes for all user nodes
2. **Pass 2**: Wire outputs (tracking them for orphan detection)
   - Outputs create file nodes and add to command outputs
   - Nodes without outputs auto-generate stamp files: `{stamp_dir}/{tag}.stamp`
3. **Pass 3**: Wire inputs and explicit dependencies
   - Inputs create file nodes and add to command inputs
   - Root nodes (no inputs/deps) depend on `sync::entry_stamp`
   - Explicit deps link to dependency's first output or stamp file
4. **Pass 4**: Connect only orphan outputs to `sync::user`
   - Outputs with no consumers are wired to `sync::user`
   - If no orphans exist, `sync::user` depends on `sync::entry_stamp` for ordering

### Key Behavior
- Nodes are added during `configure()`, not `build()`
- File nodes deduplicated per-package via `file_to_graph_id` hash table
- Stamp files enable ordering for nodes without explicit outputs
- `spn_node_link(from, to)` makes `to` wait for `from`

## Bug Fixes
1. **Cross-package dependency context** (`spn.c:2247`) - Use `dep_handle.ctx` instead of `ctx` when looking up dependency's output file, ensuring correct file node deduplication across packages
2. **Bounds check** (`spn.c:2134`) - Added `SP_ASSERT(node.index < sp_da_size(node.ctx->user_nodes))` in `spn_find_user_node`
3. **Comments removed** - Removed inline comments from `spn.h` and `spn.c` per style guide

## Test Projects (`test/manual/graph/`)

### basic_node
Single node generates `version.h` with constants; main.c uses them with static assertions.

### chained_nodes
Two nodes sharing intermediate file:
- `phase1`: produces `intermediate.h`
- `phase2`: consumes `intermediate.h`, produces `final.h`

### node_linking
Explicit `spn_node_link()` without shared files:
- `setup`: no outputs, uses stamp file
- `codegen`: depends on setup, produces header

## Context
spn is a package manager and build tool for C projects. Uses sp.h (single-header C stdlib replacement). Build DAG represented by `spn_build_graph_t` from `source/graph.h`. User nodes integrate into this graph alongside standard compilation steps.
