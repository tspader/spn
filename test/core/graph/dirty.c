#include "graph.h"

typedef struct {
  const c8* label;
  const graph_def_t* graph;
  const c8* touch [GRAPH_MAX_TOUCH];
  const c8* dirty [GRAPH_MAX_NODES];
  const c8* clean [GRAPH_MAX_NODES];
  spn_bg_err_kind_t errors [GRAPH_MAX_NODES];
} dirty_test_t;

void expect_node_dirty(s32* utest_result, built_graph_t* b, spn_bg_dirty_t* dirty, const c8* id, bool want_dirty) {
  graph_ref_t* ref = graph_ref(b, id);
  bool is_dirty =
    (ref->kind == NODE_KIND_FILE)
      ? spn_bg_is_file_dirty(dirty, ref->handle)
      : spn_bg_is_cmd_dirty(dirty, ref->handle);

  if (is_dirty != want_dirty) {
    utest_kv("node", sp_cstr_as_str(id));
    utest_fail(utest_result, __FILE__, __LINE__,
      sp_str_view(want_dirty ? "dirty" : "clean"),
      sp_str_view(is_dirty ? "dirty" : "clean"));
  }
}

void run_dirty_test(s32* utest_result, sp_test_file_manager_t* fm, dirty_test_t t) {
  built_graph_t b = build_graph(fm, t.label, t.graph);
  apply_touches(&b, t.touch);

  spn_bg_dirty_t* dirty = spn_bg_compute_dirty(b.graph);

  for (s32 i = 0; i < GRAPH_MAX_NODES; i++) {
    if (t.errors[i] == SPN_BG_OK) break;
    EXPECT_TRUE((s32)sp_da_size(dirty->errors) > i);
    if ((s32)sp_da_size(dirty->errors) > i) {
      EXPECT_TRUE(t.errors[i] == dirty->errors[i].kind);
    }
  }

  for (s32 i = 0; i < GRAPH_MAX_NODES && t.dirty[i]; i++) {
    expect_node_dirty(utest_result, &b, dirty, t.dirty[i], true);
  }
  for (s32 i = 0; i < GRAPH_MAX_NODES && t.clean[i]; i++) {
    expect_node_dirty(utest_result, &b, dirty, t.clean[i], false);
  }
}

UTEST_F(graph, missing_input_errors) {
  SKIP_ON_WASM()
  SKIP_ON_FREESTANDING()
  run_dirty_test(&ur, &ut.file_manager, (dirty_test_t) {
    .label = "missing_input_errors",
    .graph = &simple_linear_graph,
    .errors = { SPN_BG_ERR_MISSING_INPUT },
  });
}

UTEST_F(graph, missing_output_is_dirty) {
  SKIP_ON_WASM()
  SKIP_ON_FREESTANDING()
  run_dirty_test(&ur, &ut.file_manager, (dirty_test_t) {
    .label = "missing_output_is_dirty",
    .graph = &simple_linear_graph,
    .touch = { "a" },
    .dirty = { "b", "compile" },
    .clean = { "a" },
  });
}

UTEST_F(graph, input_newer_than_output_is_dirty) {
  SKIP_ON_WASM()
  SKIP_ON_FREESTANDING()
  run_dirty_test(&ur, &ut.file_manager, (dirty_test_t) {
    .label = "input_newer_than_output_is_dirty",
    .graph = &simple_linear_graph,
    .touch = { "b", "a" },
    .dirty = { "b", "compile" },
    .clean = { "a" },
  });
}

UTEST_F(graph, output_newer_than_input_is_clean) {
  SKIP_ON_WASM()
  SKIP_ON_FREESTANDING()
  run_dirty_test(&ur, &ut.file_manager, (dirty_test_t) {
    .label = "output_newer_than_input_is_clean",
    .graph = &simple_linear_graph,
    .touch = { "a", "b" },
    .clean = { "b", "a", "compile" },
  });
}

UTEST_F(graph, long_linear_dirty_propagates) {
  SKIP_ON_WASM()
  SKIP_ON_FREESTANDING()
  run_dirty_test(&ur, &ut.file_manager, (dirty_test_t) {
    .label = "long_linear_dirty_propagates",
    .graph = &long_linear_graph,
    .touch = { "b", "c", "a" },
    .dirty = { "b", "c", "compile_b", "compile_c" },
    .clean = { "a" },
  });
}

UTEST_F(graph, long_linear_partial) {
  SKIP_ON_WASM()
  SKIP_ON_FREESTANDING()
  run_dirty_test(&ur, &ut.file_manager, (dirty_test_t) {
    .label = "long_linear_partial",
    .graph = &long_linear_graph,
    .touch = { "a", "b", "c", "b" },
    .dirty = { "c", "compile_c" },
    .clean = { "a", "b", "compile_b" },
  });
}

UTEST_F(graph, diamond_propagation) {
  SKIP_ON_WASM()
  SKIP_ON_FREESTANDING()
  run_dirty_test(&ur, &ut.file_manager, (dirty_test_t) {
    .label = "diamond_propagation",
    .graph = &diamond_graph,
    .touch = { "a" },
    .dirty = { "b", "c", "d", "compile_b", "compile_c", "join_d" },
    .clean = { "a" },
  });
}

UTEST_F(graph, fork_join_partial_missing) {
  SKIP_ON_WASM()
  SKIP_ON_FREESTANDING()
  run_dirty_test(&ur, &ut.file_manager, (dirty_test_t) {
    .label = "fork_join_partial_missing",
    .graph = &fork_join_graph,
    .touch = { "c", "e", "d", "a" },
    .dirty = { "b", "d", "compile_b", "join_d" },
    .clean = { "a", "c", "e", "compile_e" },
  });
}

UTEST_F(graph, fork_join_partial_rhs) {
  SKIP_ON_WASM()
  SKIP_ON_FREESTANDING()
  run_dirty_test(&ur, &ut.file_manager, (dirty_test_t) {
    .label = "fork_join_partial_rhs",
    .graph = &fork_join_graph,
    .touch = { "a", "c", "e", "d", "b" },
    .dirty = { "d", "join_d" },
    .clean = { "a", "c", "e", "b", "compile_b", "compile_e" },
  });
}

UTEST_F(graph, fork_join_partial_lhs) {
  SKIP_ON_WASM()
  SKIP_ON_FREESTANDING()
  run_dirty_test(&ur, &ut.file_manager, (dirty_test_t) {
    .label = "fork_join_partial_lhs",
    .graph = &fork_join_graph,
    .touch = { "c", "a", "b", "d", "e" },
    .dirty = { "d", "join_d" },
    .clean = { "a", "c", "e", "b", "compile_b", "compile_e" },
  });
}

UTEST_F(graph, multi_output_missing_peer) {
  SKIP_ON_WASM()
  SKIP_ON_FREESTANDING()
  run_dirty_test(&ur, &ut.file_manager, (dirty_test_t) {
    .label = "multi_output_missing_peer",
    .graph = &multi_output_graph,
    .touch = { "a", "c" },
    .dirty = { "b", "c", "split" },
    .clean = { "a" },
  });
}

UTEST_F(graph, split_join_asymmetric) {
  SKIP_ON_WASM()
  SKIP_ON_FREESTANDING()
  run_dirty_test(&ur, &ut.file_manager, (dirty_test_t) {
    .label = "split_join_asymmetric",
    .graph = &split_join_graph,
    .touch = { "a", "b", "c", "d", "e", "f", "a" },
    .dirty = { "f", "join_f" },
  });
}

UTEST_F(graph, asymmetric_long_fork_dirty) {
  SKIP_ON_WASM()
  SKIP_ON_FREESTANDING()
  run_dirty_test(&ur, &ut.file_manager, (dirty_test_t) {
    .label = "asymmetric_long_fork_dirty",
    .graph = &asymmetric_fork_graph,
    .touch = { "a", "b", "c", "d", "e", "f", "g", "a" },
    .dirty = { "b", "c", "d", "e", "g", "compile_b", "compile_c", "compile_d", "compile_e", "join_g" },
    .clean = { "f" },
  });
}

UTEST_F(graph, comb_all_dirty) {
  SKIP_ON_WASM()
  SKIP_ON_FREESTANDING()
  run_dirty_test(&ur, &ut.file_manager, (dirty_test_t) {
    .label = "comb_all_dirty",
    .graph = &comb_graph,
    .touch = { "a" },
    .dirty = { "b", "c", "d", "compile_b", "compile_c", "compile_d" },
    .clean = { "a" },
  });
}

UTEST_F(graph, comb_partial_dirty) {
  SKIP_ON_WASM()
  SKIP_ON_FREESTANDING()
  run_dirty_test(&ur, &ut.file_manager, (dirty_test_t) {
    .label = "comb_partial_dirty",
    .graph = &comb_graph,
    .touch = { "a", "b", "c", "d", "c" },
    .dirty = { "d", "compile_d" },
    .clean = { "a", "b", "c", "compile_b", "compile_c" },
  });
}

UTEST_F(graph, no_input_missing_output) {
  SKIP_ON_WASM()
  SKIP_ON_FREESTANDING()
  run_dirty_test(&ur, &ut.file_manager, (dirty_test_t) {
    .label = "no_input_missing_output",
    .graph = &no_input_graph,
    .dirty = { "a", "b", "generate_a", "compile_b" },
  });
}
