#include "graph.h"

typedef struct {
  u32 files;
  u32 cmds;
} visit_count_t;

static void count_file_fn(spn_build_graph_t* graph, spn_bg_file_t* file, void* user_data) {
  (void)graph;
  (void)file;
  ((visit_count_t*)user_data)->files++;
}

static void count_cmd_fn(spn_build_graph_t* graph, spn_bg_cmd_t* cmd, void* user_data) {
  (void)graph;
  (void)cmd;
  ((visit_count_t*)user_data)->cmds++;
}

typedef struct {
  const c8* name;
  const graph_def_t* graph;
  spn_bg_it_mode_t mode;
  spn_bg_it_dir_t direction;
  visit_count_t expect;
} traversal_test_t;

static const traversal_test_t tests [] = {
  {
    .name = "dfs_out_to_in",
    .graph = &simple_linear_graph,
    .mode = SPN_BG_ITER_MODE_DEPTH_FIRST,
    .direction = SPN_BG_ITER_DIR_OUT_TO_IN,
    .expect = { .files = 2, .cmds = 1 },
  },
  {
    .name = "bfs_out_to_in",
    .graph = &simple_linear_graph,
    .mode = SPN_BG_ITER_MODE_BREADTH_FIRST,
    .direction = SPN_BG_ITER_DIR_OUT_TO_IN,
    .expect = { .files = 2, .cmds = 1 },
  },
  {
    .name = "dfs_in_to_out",
    .graph = &simple_linear_graph,
    .mode = SPN_BG_ITER_MODE_DEPTH_FIRST,
    .direction = SPN_BG_ITER_DIR_IN_TO_OUT,
    .expect = { .files = 2, .cmds = 1 },
  },
};

sp_test_each(graph, visit_once, traversal_test_t, tests, .setup = graph_setup) {
  built_graph_t b = build_graph(t, it->graph);
  visit_count_t counter = sp_zero;

  spn_bg_it_config_t config = {
    .graph = b.graph,
    .direction = it->direction,
    .on_cmd = count_cmd_fn,
    .on_file = count_file_fn,
    .user_data = &counter,
  };

  switch (it->mode) {
    case SPN_BG_ITER_MODE_DEPTH_FIRST:   spn_bg_dfs(config); break;
    case SPN_BG_ITER_MODE_BREADTH_FIRST: spn_bg_bfs(config); break;
  }

  sp_expect_eq(t, counter.files, it->expect.files);
  sp_expect_eq(t, counter.cmds, it->expect.cmds);

  return SP_OK;
}
