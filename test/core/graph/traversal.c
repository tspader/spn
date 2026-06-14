#include "graph.h"

typedef struct {
  u32 file_count;
  u32 cmd_count;
} visit_counter_t;

void count_file_fn(spn_build_graph_t* graph, spn_bg_file_t* file, void* user_data) {
  (void)graph;
  (void)file;
  ((visit_counter_t*)user_data)->file_count++;
}

void count_cmd_fn(spn_build_graph_t* graph, spn_bg_cmd_t* cmd, void* user_data) {
  (void)graph;
  (void)cmd;
  ((visit_counter_t*)user_data)->cmd_count++;
}

typedef struct {
  const c8* label;
  const graph_def_t* graph;
  spn_bg_it_mode_t mode;
  spn_bg_it_dir_t direction;
  u32 expected_files;
  u32 expected_cmds;
} traversal_test_t;

void run_traversal_test(s32* utest_result, sp_test_file_manager_t* fm, traversal_test_t t) {
  built_graph_t b = build_graph(fm, t.label, t.graph);
  visit_counter_t counter = sp_zero;

  spn_bg_it_config_t config = {
    .graph = b.graph,
    .direction = t.direction,
    .on_cmd = count_cmd_fn,
    .on_file = count_file_fn,
    .user_data = &counter,
  };

  switch (t.mode) {
    case SPN_BG_ITER_MODE_DEPTH_FIRST:   spn_bg_dfs(config); break;
    case SPN_BG_ITER_MODE_BREADTH_FIRST: spn_bg_bfs(config); break;
  }

  EXPECT_EQ(counter.file_count, t.expected_files);
  EXPECT_EQ(counter.cmd_count, t.expected_cmds);
}

UTEST_F(graph, visit_once_dfs_out_to_in) {
  SKIP_ON_WASM()
  SKIP_ON_FREESTANDING()
  run_traversal_test(&ur, &ut.file_manager, (traversal_test_t) {
    .label = "visit_once_dfs_out_to_in",
    .graph = &simple_linear_graph,
    .mode = SPN_BG_ITER_MODE_DEPTH_FIRST,
    .direction = SPN_BG_ITER_DIR_OUT_TO_IN,
    .expected_files = 2,
    .expected_cmds = 1,
  });
}

UTEST_F(graph, visit_once_bfs_out_to_in) {
  SKIP_ON_WASM()
  SKIP_ON_FREESTANDING()
  run_traversal_test(&ur, &ut.file_manager, (traversal_test_t) {
    .label = "visit_once_bfs_out_to_in",
    .graph = &simple_linear_graph,
    .mode = SPN_BG_ITER_MODE_BREADTH_FIRST,
    .direction = SPN_BG_ITER_DIR_OUT_TO_IN,
    .expected_files = 2,
    .expected_cmds = 1,
  });
}

UTEST_F(graph, visit_once_dfs_in_to_out) {
  SKIP_ON_WASM()
  SKIP_ON_FREESTANDING()
  run_traversal_test(&ur, &ut.file_manager, (traversal_test_t) {
    .label = "visit_once_dfs_in_to_out",
    .graph = &simple_linear_graph,
    .mode = SPN_BG_ITER_MODE_DEPTH_FIRST,
    .direction = SPN_BG_ITER_DIR_IN_TO_OUT,
    .expected_files = 2,
    .expected_cmds = 1,
  });
}

