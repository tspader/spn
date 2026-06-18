#ifndef SP_TEST_GRAPH_H
#define SP_TEST_GRAPH_H

#include "sp/sp_graph.h"

#include "test.h"
#include "utest.h"

#define GRAPH_MAX_NODES 16
#define GRAPH_MAX_EDGES 6
#define GRAPH_MAX_TOUCH 16

typedef enum {
  NODE_KIND_FILE,
  NODE_KIND_COMMAND,
} node_kind_t;

typedef struct {
  const c8* id;
  node_kind_t kind;
  const c8* inputs [GRAPH_MAX_EDGES];
  const c8* outputs [GRAPH_MAX_EDGES];
} graph_node_t;

typedef struct {
  graph_node_t nodes[GRAPH_MAX_NODES];
} graph_def_t;

typedef struct {
  spn_bg_id_t handle;
  node_kind_t kind;
} graph_ref_t;

typedef struct {
  spn_build_graph_t* graph;
  const graph_def_t* def;
  sp_str_ht(graph_ref_t) refs;
} built_graph_t;

s32 build_fn_noop(spn_bg_cmd_t* cmd, void* ud) {
  (void)cmd;
  (void)ud;
  return 0;
}

graph_ref_t* graph_ref(built_graph_t* b, const c8* id) {
  graph_ref_t* ref = sp_str_ht_get(b->refs, sp_cstr_as_str(id));
  SP_ASSERT(ref);
  return ref;
}

built_graph_t build_graph(sp_test_file_manager_t* fm, const c8* label, const graph_def_t* def) {
  built_graph_t b = sp_zero;
  b.def = def;
  b.graph = spn_bg_new(fm->mem);
  sp_str_ht_init(fm->mem, b.refs);

  sp_str_t sandbox = sp_test_file_path(fm, sp_cstr_as_str(label));
  sp_fs_create_dir(sandbox);

  // pass 1: create files and commands, recording the handle of each node
  for (s32 i = 0; i < GRAPH_MAX_NODES && def->nodes[i].id; i++) {
    const graph_node_t* n = &def->nodes[i];
    graph_ref_t ref = { sp_zero, n->kind };
    switch (n->kind) {
      case NODE_KIND_FILE: {
        sp_str_t path = sp_fs_join_path(fm->mem, sandbox, sp_cstr_as_str(n->id));
        ref.handle = spn_bg_add_file(b.graph, path);
        break;
      }
      case NODE_KIND_COMMAND: {
        ref.handle = spn_bg_add_fn(b.graph, build_fn_noop, SP_NULLPTR);
        spn_bg_tag_command_c(b.graph, ref.handle, n->id);
        break;
      }
    }
    sp_str_ht_insert(b.refs, sp_cstr_as_str(n->id), ref);
  }

  // pass 2: wire edges (commands declare both their inputs and outputs)
  for (s32 i = 0; i < GRAPH_MAX_NODES && def->nodes[i].id; i++) {
    const graph_node_t* n = &def->nodes[i];
    if (n->kind != NODE_KIND_COMMAND) continue;

    for (s32 j = 0; j < GRAPH_MAX_EDGES && n->inputs[j]; j++) {
      spn_bg_cmd_add_input(b.graph, graph_ref(&b, n->id)->handle, graph_ref(&b, n->inputs[j])->handle);
    }
    for (s32 j = 0; j < GRAPH_MAX_EDGES && n->outputs[j]; j++) {
      spn_bg_cmd_add_output(b.graph, graph_ref(&b, n->id)->handle, graph_ref(&b, n->outputs[j])->handle);
    }
  }

  return b;
}

void touch_file(sp_str_t path) {
  sp_io_file_writer_t w = sp_zero;
  sp_io_file_writer_from_path(&w, path);
  sp_io_write(&w.base, " ", 1, SP_NULLPTR);
  sp_io_file_writer_close(&w);

  sp_os_sleep_ms(50);
}

void touch_node(built_graph_t* b, const c8* id) {
  spn_bg_file_t* file = spn_bg_find_file(b->graph, graph_ref(b, id)->handle);
  SP_ASSERT(file);
  touch_file(file->path);
}

void apply_touches(built_graph_t* b, const c8* const* touch) {
  for (s32 i = 0; i < GRAPH_MAX_TOUCH && touch[i]; i++) {
    touch_node(b, touch[i]);
  }
}


/////////////
// FIXTURE //
/////////////

#define uf utest_fixture

struct graph {
  sp_test_file_manager_t file_manager;
};

UTEST_F_SETUP(graph) {
  SKIP_ON_WASM()
  SKIP_ON_FREESTANDING()
  sp_test_file_manager_init(&uf->file_manager);
}

UTEST_F_TEARDOWN(graph) {
  SKIP_ON_WASM()
  SKIP_ON_FREESTANDING()
  sp_test_file_manager_cleanup(&uf->file_manager);
}


////////////
// GRAPHS //
////////////

// ┌───┐     ┌─────────┐     ┌───┐
// │ a │────▶│ compile │────▶│ b │
// └───┘     └─────────┘     └───┘
static const graph_def_t simple_linear_graph = {{
  { "a", NODE_KIND_FILE },
  { "b", NODE_KIND_FILE },
  { "compile", NODE_KIND_COMMAND, { "a" }, { "b" } },
}};

// ┌───┐     ┌─────────┐     ┌───┐     ┌─────────┐     ┌───┐
// │ a │────▶│ compile │────▶│ b │────▶│ compile │────▶│ c │
// └───┘     └─────────┘     └───┘     └─────────┘     └───┘
static const graph_def_t long_linear_graph = {{
  { "a", NODE_KIND_FILE },
  { "b", NODE_KIND_FILE },
  { "c", NODE_KIND_FILE },
  { "compile_b", NODE_KIND_COMMAND, { "a" }, { "b" } },
  { "compile_c", NODE_KIND_COMMAND, { "b" }, { "c" } },
}};

// ┌───┐     ┌─────────┐     ┌───┐
// │ a │────▶│ compile │────▶│ b │──┐
// └───┘     └─────────┘     └───┘  │  ┌──────┐     ┌───┐
//                                  ├─▶│ join │────▶│ d │
// ┌───┐     ┌─────────┐     ┌───┐  │  └──────┘     └───┘
// │ c │────▶│ compile │────▶│ e │──┘
// └───┘     └─────────┘     └───┘
static const graph_def_t fork_join_graph = {{
  { "a", NODE_KIND_FILE },
  { "b", NODE_KIND_FILE },
  { "c", NODE_KIND_FILE },
  { "d", NODE_KIND_FILE },
  { "e", NODE_KIND_FILE },
  { "compile_b", NODE_KIND_COMMAND, { "a" }, { "b" } },
  { "compile_e", NODE_KIND_COMMAND, { "c" }, { "e" } },
  { "join_d", NODE_KIND_COMMAND, { "b", "e" }, { "d" } },
}};

//                       ┌───┐
//                    ┌─▶│ b │──┐
// ┌───┐     ┌───────┐│  └───┘  │  ┌──────┐     ┌───┐
// │ a │────▶│ split │┤         ├─▶│ join │────▶│ d │──┐
// └───┘     └───────┘│  ┌───┐  │  └──────┘     └───┘  │  ┌──────┐     ┌───┐
//                    └─▶│ c │──┘                      ├─▶│ join │────▶│ f │
//                       └───┘                         │  └──────┘     └───┘
//                                              ┌───┐  │
//                                              │ e │──┘
//                                              └───┘
static const graph_def_t split_join_graph = {{
  { "a", NODE_KIND_FILE },
  { "b", NODE_KIND_FILE },
  { "c", NODE_KIND_FILE },
  { "d", NODE_KIND_FILE },
  { "e", NODE_KIND_FILE },
  { "f", NODE_KIND_FILE },
  { "split", NODE_KIND_COMMAND, { "a" }, { "b", "c" } },
  { "join_d", NODE_KIND_COMMAND, { "b", "c" }, { "d" } },
  { "join_f", NODE_KIND_COMMAND, { "d", "e" }, { "f" } },
}};

//          ┌─────────┐     ┌───┐
//       ┌─▶│ compile │────▶│ b │──┐
// ┌───┐ │  └─────────┘     └───┘  │  ┌──────┐     ┌───┐
// │ a │─┤                         ├─▶│ join │────▶│ d │
// └───┘ │  ┌─────────┐     ┌───┐  │  └──────┘     └───┘
//       └─▶│ compile │────▶│ c │──┘
//          └─────────┘     └───┘
static const graph_def_t diamond_graph = {{
  { "a", NODE_KIND_FILE },
  { "b", NODE_KIND_FILE },
  { "c", NODE_KIND_FILE },
  { "d", NODE_KIND_FILE },
  { "compile_b", NODE_KIND_COMMAND, { "a" }, { "b" } },
  { "compile_c", NODE_KIND_COMMAND, { "a" }, { "c" } },
  { "join_d", NODE_KIND_COMMAND, { "b", "c" }, { "d" } },
}};

// a -> b -> c -> d -> e  (long chain), then (e, f) -> g
static const graph_def_t asymmetric_fork_graph = {{
  { "a", NODE_KIND_FILE },
  { "b", NODE_KIND_FILE },
  { "c", NODE_KIND_FILE },
  { "d", NODE_KIND_FILE },
  { "e", NODE_KIND_FILE },
  { "f", NODE_KIND_FILE },
  { "g", NODE_KIND_FILE },
  { "compile_b", NODE_KIND_COMMAND, { "a" }, { "b" } },
  { "compile_c", NODE_KIND_COMMAND, { "b" }, { "c" } },
  { "compile_d", NODE_KIND_COMMAND, { "c" }, { "d" } },
  { "compile_e", NODE_KIND_COMMAND, { "d" }, { "e" } },
  { "join_g", NODE_KIND_COMMAND, { "e", "f" }, { "g" } },
}};

//                       ┌───┐     ┌─────────┐     ┌───┐
//                    ┌─▶│ b │────▶│ compile │────▶│ d │
// ┌───┐     ┌───────┐│  └───┘     └─────────┘     └───┘
// │ a │────▶│ split │┤
// └───┘     └───────┘│  ┌───┐     ┌─────────┐     ┌───┐
//                    └─▶│ c │────▶│ compile │────▶│ e │
//                       └───┘     └─────────┘     └───┘
static const graph_def_t multi_output_graph = {{
  { "a", NODE_KIND_FILE },
  { "b", NODE_KIND_FILE },
  { "c", NODE_KIND_FILE },
  { "d", NODE_KIND_FILE },
  { "e", NODE_KIND_FILE },
  { "split", NODE_KIND_COMMAND, { "a" }, { "b", "c" } },
  { "compile_d", NODE_KIND_COMMAND, { "b" }, { "d" } },
  { "compile_e", NODE_KIND_COMMAND, { "c" }, { "e" } },
}};

// ┌───┐
// │ a │─┬──────────────────────┬───────────────────────┐
// └───┘ ▼                      ▼                        ▼
//   ┌─────────┐  ┌───┐  ┌─────────┐  ┌───┐  ┌─────────┐  ┌───┐
//   │ compile │─▶│ b │─▶│ compile │─▶│ c │─▶│ compile │─▶│ d │
//   └─────────┘  └───┘  └─────────┘  └───┘  └─────────┘  └───┘
static const graph_def_t comb_graph = {{
  { "a", NODE_KIND_FILE },
  { "b", NODE_KIND_FILE },
  { "c", NODE_KIND_FILE },
  { "d", NODE_KIND_FILE },
  { "compile_b", NODE_KIND_COMMAND, { "a" }, { "b" } },
  { "compile_c", NODE_KIND_COMMAND, { "b", "a" }, { "c" } },
  { "compile_d", NODE_KIND_COMMAND, { "c", "a" }, { "d" } },
}};

// ┌─────────┐     ┌───┐     ┌─────────┐     ┌───┐
// │ generate│────▶│ a │────▶│ compile │────▶│ b │
// └─────────┘     └───┘     └─────────┘     └───┘
static const graph_def_t no_input_graph = {{
  { "a", NODE_KIND_FILE },
  { "b", NODE_KIND_FILE },
  { "generate_a", NODE_KIND_COMMAND, { 0 }, { "a" } },
  { "compile_b", NODE_KIND_COMMAND, { "a" }, { "b" } },
}};

#endif
