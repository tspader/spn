#include "graph.h"

sp_test_suite(graph, .serial = true);

sp_err_t graph_setup(sp_test_t* t) {
#if defined(SP_WASM) || defined(SP_FREESTANDING)
  return sp_test_skip(t, "unimplemented on this platform");
#else
  (void)t;
  return SP_OK;
#endif
}

static s32 build_fn_noop(spn_bg_cmd_t* cmd, void* ud) {
  (void)cmd;
  (void)ud;
  return 0;
}

graph_ref_t* graph_ref(built_graph_t* b, const c8* id) {
  graph_ref_t* ref = sp_str_ht_get(b->refs, sp_cstr_as_str(id));
  SP_ASSERT(ref);
  return ref;
}

built_graph_t graph_build(sp_test_t* t, const graph_def_t* def) {
  sp_mem_t mem = sp_test_arena(t);

  built_graph_t b = sp_zero;
  b.def = def;
  b.graph = spn_bg_new(mem);
  sp_str_ht_init(mem, b.refs);

  sp_str_t sandbox = sp_test_dir(t);

  // pass 1: create files and commands, recording the handle of each node
  sp_carr_for(def->nodes, i) {
    const graph_node_t* n = &def->nodes[i];
    if (!n->id) break;

    graph_ref_t ref = { sp_zero, n->kind };
    switch (n->kind) {
      case NODE_KIND_FILE: {
        sp_str_t path = sp_fs_join_path(mem, sandbox, sp_cstr_as_str(n->id));
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
  sp_carr_for(def->nodes, i) {
    const graph_node_t* n = &def->nodes[i];
    if (!n->id) break;
    if (n->kind != NODE_KIND_COMMAND) continue;

    sp_carr_for(n->inputs, j) {
      if (!n->inputs[j]) break;
      spn_bg_cmd_add_input(b.graph, graph_ref(&b, n->id)->handle, graph_ref(&b, n->inputs[j])->handle);
    }
    sp_carr_for(n->outputs, j) {
      if (!n->outputs[j]) break;
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
  sp_for(i, GRAPH_MAX_TOUCH) {
    if (!touch[i]) break;
    touch_node(b, touch[i]);
  }
}


////////////
// GRAPHS //
////////////

// ┌───┐     ┌─────────┐     ┌───┐
// │ a │────▶│ compile │────▶│ b │
// └───┘     └─────────┘     └───┘
const graph_def_t simple_linear_graph = {{
  { "a", NODE_KIND_FILE },
  { "b", NODE_KIND_FILE },
  { "compile", NODE_KIND_COMMAND, { "a" }, { "b" } },
}};

// ┌───┐     ┌─────────┐     ┌───┐     ┌─────────┐     ┌───┐
// │ a │────▶│ compile │────▶│ b │────▶│ compile │────▶│ c │
// └───┘     └─────────┘     └───┘     └─────────┘     └───┘
const graph_def_t long_linear_graph = {{
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
const graph_def_t fork_join_graph = {{
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
const graph_def_t split_join_graph = {{
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
const graph_def_t diamond_graph = {{
  { "a", NODE_KIND_FILE },
  { "b", NODE_KIND_FILE },
  { "c", NODE_KIND_FILE },
  { "d", NODE_KIND_FILE },
  { "compile_b", NODE_KIND_COMMAND, { "a" }, { "b" } },
  { "compile_c", NODE_KIND_COMMAND, { "a" }, { "c" } },
  { "join_d", NODE_KIND_COMMAND, { "b", "c" }, { "d" } },
}};

// a -> b -> c -> d -> e  (long chain), then (e, f) -> g
const graph_def_t asymmetric_fork_graph = {{
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
const graph_def_t multi_output_graph = {{
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
const graph_def_t comb_graph = {{
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
const graph_def_t no_input_graph = {{
  { "a", NODE_KIND_FILE },
  { "b", NODE_KIND_FILE },
  { "generate_a", NODE_KIND_COMMAND, { 0 }, { "a" } },
  { "compile_b", NODE_KIND_COMMAND, { "a" }, { "b" } },
}};
