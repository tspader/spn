#define SP_IMPLEMENTATION
#include "sp.h"
#include "graph.h"

#define SP_TEST_IMPLEMENTATION
#include "test.h"
#include "utest.h"


void touch_file(sp_str_t path) {
  // @spader: i tried to be smart and poll, but it was junk
  if (sp_fs_exists(path)) {
    sp_os_sleep_ms(100);
  }

  sp_io_stream_t s = sp_io_from_file(path, SP_IO_MODE_APPEND);
  sp_io_write_str(&s, sp_str_lit(" "));
  sp_io_close(&s);
}

void touch_node(spn_build_graph_t* graph, spn_bg_id_t id) {
  spn_build_file_t* file = spn_bg_find_file(graph, id);
  SP_ASSERT(file);
  touch_file(file->path);
}



///////////////////////
// BUILD COMMAND FNS //
///////////////////////
sp_mutex_t g_log_mutex;

void build_fn_noop(spn_build_cmd_t* cmd, void* ud) {

}

#define uf utest_fixture



////////////
// GRAPHS //
////////////
// ┌───┐     ┌───────┐     ┌───┐
// │ a │────▶│ touch │────▶│ b │
// └───┘     └───────┘     └───┘
typedef struct {
  spn_build_graph_t* graph;
  spn_bg_id_t a;
  spn_bg_id_t b;
  spn_bg_id_t touch;
} short_linear_graph_t;

short_linear_graph_t create_short_linear_graph() {
  short_linear_graph_t g;
  g.graph = spn_bg_new();
  g.a = spn_bg_add_file(g.graph, sp_str_lit("a"));
  g.b = spn_bg_add_file(g.graph, sp_str_lit("b"));
  g.touch = spn_bg_add_command(g.graph, SPN_BUILD_CMD_FN);

  spn_build_cmd_t* touch = spn_bg_find_command(g.graph, g.touch);
  spn_build_command_add_input(g.graph, g.touch, g.a);
  spn_build_command_add_output(g.graph, g.touch, g.b);

  return g;
}

// ┌───┐     ┌──────┐     ┌───┐     ┌──────┐     ┌───┐
// │ a │────▶│ cmd1 │────▶│ b │────▶│ cmd2 │────▶│ c │
// └───┘     └──────┘     └───┘     └──────┘     └───┘
typedef struct {
  spn_build_graph_t* graph;
  spn_bg_id_t a, b, c;
  spn_bg_id_t cmd1, cmd2;
} long_linear_graph_t;

long_linear_graph_t create_long_linear_graph() {
  long_linear_graph_t r;
  r.graph = spn_bg_new();
  r.a = spn_bg_add_file(r.graph, sp_str_lit("a"));
  r.b = spn_bg_add_file(r.graph, sp_str_lit("b"));
  r.c = spn_bg_add_file(r.graph, sp_str_lit("c"));

  r.cmd1 = spn_bg_add_command(r.graph, SPN_BUILD_CMD_FN);
  r.cmd2 = spn_bg_add_command(r.graph, SPN_BUILD_CMD_FN);

  spn_build_command_add_input(r.graph, r.cmd1, r.a);
  spn_build_file_set_command(r.graph, r.b, r.cmd1);

  spn_build_command_add_input(r.graph, r.cmd2, r.b);
  spn_build_file_set_command(r.graph, r.c, r.cmd2);

  return r;
}

 // ┌────────┐     ┌─────┐     ┌────────┐
 // │ main.c │────▶│ gcc │────▶│ main.o │──┐
 // └────────┘     └─────┘     └────────┘  │     ┌────┐     ┌─────────────┐
 //                                        ├────▶│ ld │────▶│ program.exe │
 // ┌─────────┐     ┌─────┐     ┌─────────┐│     └────┘     └─────────────┘
 // │ utils.c │────▶│ gcc │────▶│ utils.o │┘
 // └─────────┘     └─────┘     └─────────┘
typedef struct {
  spn_build_graph_t* graph;
  spn_bg_id_t src1, src2, obj1, obj2, exe;
  spn_bg_id_t compile1, compile2, link;
} fork_join_graph_t;

fork_join_graph_t create_fork_join_graph() {
  fork_join_graph_t r;
  r.graph = spn_bg_new();

  r.exe = spn_bg_add_file(r.graph, sp_str_lit("program.exe"));
  r.obj1 = spn_bg_add_file(r.graph, sp_str_lit("main.o"));
  r.obj2 = spn_bg_add_file(r.graph, sp_str_lit("utils.o"));
  r.src1 = spn_bg_add_file(r.graph, sp_str_lit("main.c"));
  r.src2 = spn_bg_add_file(r.graph, sp_str_lit("utils.c"));

  r.link = spn_bg_add_command(r.graph, SPN_BUILD_CMD_FN);
  r.compile1 = spn_bg_add_command(r.graph, SPN_BUILD_CMD_FN);
  r.compile2 = spn_bg_add_command(r.graph, SPN_BUILD_CMD_FN);

  spn_bg_tag_command_c(r.graph, r.link, "ld");
  spn_bg_tag_command_c(r.graph, r.compile1, "gcc");
  spn_bg_tag_command_c(r.graph, r.compile2, "gcc");

  spn_build_command_add_input(r.graph, r.link, r.obj1);
  spn_build_command_add_input(r.graph, r.link, r.obj2);
  spn_build_command_add_output(r.graph, r.link, r.exe);

  spn_build_command_add_input(r.graph, r.compile1, r.src1);
  spn_build_command_add_output(r.graph, r.compile1, r.obj1);

  spn_build_command_add_input(r.graph, r.compile2, r.src2);
  spn_build_command_add_output(r.graph, r.compile2, r.obj2);

  return r;
}

 //               ┌────┐
 //            ┌─▶│ td │──┐
 // ┌──────┐   │  └────┘  │  ┌──────┐     ┌──────┐
 // │ tdns │──▶├          ├─▶│ join │────▶│ tdns │──┐
 // └──────┘   │  ┌────┐  │  └──────┘     └──────┘  │  ┌──────┐     ┌────────────────┐
 //            └─▶│ ns │──┘                         ├─▶│ love │────▶│ tdns loves alg │
 //               └────┘                            │  └──────┘     └────────────────┘
 //                                        ┌─────┐  │
 //                                        │ alg │──┘
 //                                        └─────┘
spn_build_graph_t* create_split_join_graph() {
  spn_build_graph_t* g = spn_bg_new();
  spn_bg_id_t tdns = spn_bg_add_file(g, sp_str_lit("tdns"));
  spn_bg_id_t td = spn_bg_add_file(g, sp_str_lit("td"));
  spn_bg_id_t ns = spn_bg_add_file(g, sp_str_lit("ns"));
  spn_bg_id_t tdns2 = spn_bg_add_file(g, sp_str_lit("tdns"));
  spn_bg_id_t alg = spn_bg_add_file(g, sp_str_lit("alg"));
  spn_bg_id_t tdns_loves_alg = spn_bg_add_file(g, sp_str_lit("tdns loves alg"));

  spn_bg_id_t split = spn_bg_add_command(g, SPN_BUILD_CMD_FN);
  spn_bg_id_t join = spn_bg_add_command(g, SPN_BUILD_CMD_FN);
  spn_bg_id_t love = spn_bg_add_command(g, SPN_BUILD_CMD_FN);

  spn_build_cmd_t* cmd = spn_bg_find_command(g, split);
  spn_bg_tag_command_c(g, split, "split");

  cmd = spn_bg_find_command(g, join);
  spn_bg_tag_command_c(g, join, "join");

  cmd = spn_bg_find_command(g, love);
  spn_bg_tag_command_c(g, love, "love");

  spn_build_command_add_input(g, split, tdns);
  spn_build_command_add_output(g, split, td);
  spn_build_command_add_output(g, split, ns);

  spn_build_command_add_input(g, join, td);
  spn_build_command_add_input(g, join, ns);
  spn_build_file_set_command(g, tdns2, join);

  spn_build_command_add_input(g, love, tdns2);
  spn_build_command_add_input(g, love, alg);
  spn_build_file_set_command(g, tdns_loves_alg, love);

  return g;
}

//          ┌───────┐     ┌───┐
//       ┌─▶│ left  │────▶│ b │──┐
// ┌───┐ │  └───────┘     └───┘  │  ┌──────┐     ┌───┐
// │ a │─┤                       ├─▶│ join │────▶│ d │
// └───┘ │  ┌───────┐     ┌───┐  │  └──────┘     └───┘
//       └─▶│ right │────▶│ c │──┘
//          └───────┘     └───┘
typedef struct {
  spn_build_graph_t* graph;
  spn_bg_id_t a, b, c, d;
  spn_bg_id_t left, right, join;
} diamond_graph_t;

diamond_graph_t create_diamond_graph() {
  diamond_graph_t r;
  r.graph = spn_bg_new();
  r.a = spn_bg_add_file(r.graph, sp_str_lit("a"));
  r.b = spn_bg_add_file(r.graph, sp_str_lit("b"));
  r.c = spn_bg_add_file(r.graph, sp_str_lit("c"));
  r.d = spn_bg_add_file(r.graph, sp_str_lit("d"));
  r.left = spn_bg_add_command(r.graph, SPN_BUILD_CMD_FN);
  r.right = spn_bg_add_command(r.graph, SPN_BUILD_CMD_FN);
  r.join = spn_bg_add_command(r.graph, SPN_BUILD_CMD_FN);

  spn_bg_tag_command_c(r.graph, r.left, "left");
  spn_bg_tag_command_c(r.graph, r.right, "right");
  spn_bg_tag_command_c(r.graph, r.join, "join");

  spn_build_command_add_input(r.graph, r.left, r.a);
  spn_build_file_set_command(r.graph, r.b, r.left);

  spn_build_command_add_input(r.graph, r.right, r.a);
  spn_build_file_set_command(r.graph, r.c, r.right);

  spn_build_command_add_input(r.graph, r.join, r.b);
  spn_build_command_add_input(r.graph, r.join, r.c);
  spn_build_file_set_command(r.graph, r.d, r.join);

  return r;
}

//                       ┌───┐     ┌────────┐     ┌───┐
//                    ┌─▶│ b │────▶│ proc_b │────▶│ d │
// ┌───┐     ┌───────┐│  └───┘     └────────┘     └───┘
// │ a │────▶│ split │┤
// └───┘     └───────┘│  ┌───┐     ┌────────┐     ┌───┐
//                    └─▶│ c │────▶│ proc_c │────▶│ e │
//                       └───┘     └────────┘     └───┘
typedef struct {
  spn_build_graph_t* graph;
  spn_bg_id_t a, b, c, d, e;
  spn_bg_id_t split, proc_b, proc_c;
} multi_output_graph_t;

multi_output_graph_t create_multi_output_graph() {
  multi_output_graph_t r;
  r.graph = spn_bg_new();
  r.a = spn_bg_add_file(r.graph, sp_str_lit("a"));
  r.b = spn_bg_add_file(r.graph, sp_str_lit("b"));
  r.c = spn_bg_add_file(r.graph, sp_str_lit("c"));
  r.d = spn_bg_add_file(r.graph, sp_str_lit("d"));
  r.e = spn_bg_add_file(r.graph, sp_str_lit("e"));

  r.split = spn_bg_add_command(r.graph, SPN_BUILD_CMD_FN);
  r.proc_b = spn_bg_add_command(r.graph, SPN_BUILD_CMD_FN);
  r.proc_c = spn_bg_add_command(r.graph, SPN_BUILD_CMD_FN);

  spn_build_command_add_input(r.graph, r.split, r.a);
  spn_build_file_set_command(r.graph, r.b, r.split);
  spn_build_file_set_command(r.graph, r.c, r.split);

  spn_build_command_add_input(r.graph, r.proc_b, r.b);
  spn_build_file_set_command(r.graph, r.d, r.proc_b);

  spn_build_command_add_input(r.graph, r.proc_c, r.c);
  spn_build_file_set_command(r.graph, r.e, r.proc_c);

  return r;
}

typedef struct {
  short_linear_graph_t short_linear;
  fork_join_graph_t fork_join;
  spn_build_graph_t* split_join;
  long_linear_graph_t long_linear;
  diamond_graph_t diamond;
  multi_output_graph_t multi_output;
} graphs_t;

void bind_graph(spn_build_graph_t* g, sp_test_file_manager_t* fm) {
  sp_da_for(g->files, i) {
    g->files[i].path = sp_test_file_path(fm, g->files[i].path);
  }

  sp_da_for(g->commands, i) {
    g->commands[i].kind = SPN_BUILD_CMD_FN;
    g->commands[i].fn.on_execute = build_fn_noop;
  }
}

graphs_t build_graphs(sp_test_file_manager_t* file_manager) {
  graphs_t graphs = {
    .short_linear = create_short_linear_graph(),
    .fork_join = create_fork_join_graph(),
    .split_join = create_split_join_graph(),
    .long_linear = create_long_linear_graph(),
    .diamond = create_diamond_graph(),
    .multi_output = create_multi_output_graph(),
  };

  bind_graph(graphs.short_linear.graph, file_manager);
  bind_graph(graphs.fork_join.graph, file_manager);
  bind_graph(graphs.split_join, file_manager);
  bind_graph(graphs.long_linear.graph, file_manager);
  bind_graph(graphs.diamond.graph, file_manager);
  bind_graph(graphs.multi_output.graph, file_manager);

  return graphs;
}



///////////////
// TRAVERSAL //
///////////////
struct spn_test_traversal {
  sp_test_file_manager_t file_manager;
  graphs_t graphs;
};

UTEST_F_SETUP(spn_test_traversal) {
  sp_test_file_manager_init(&uf->file_manager);
  uf->graphs = build_graphs(&uf->file_manager);
}

UTEST_F_TEARDOWN(spn_test_traversal) {
  sp_test_file_manager_cleanup(&uf->file_manager);
}

typedef struct {
  u32 file_count;
  u32 cmd_count;
} visit_counter_t;

void count_file_fn(spn_build_graph_t* graph, spn_build_file_t* file, void* user_data) {
  visit_counter_t* counter = (visit_counter_t*)user_data;
  counter->file_count++;
}

void count_cmd_fn(spn_build_graph_t* graph, spn_build_cmd_t* cmd, void* user_data) {
  visit_counter_t* counter = (visit_counter_t*)user_data;
  counter->cmd_count++;
}

UTEST_F(spn_test_traversal, visit_once) {
  visit_counter_t counter = SP_ZERO_INITIALIZE();

  spn_bg_dfs((spn_bg_it_config_t){
    .graph = uf->graphs.short_linear.graph,
    .direction = SPN_BG_ITER_DIR_OUT_TO_IN,
    .on_cmd = count_cmd_fn,
    .on_file = count_file_fn,
    .user_data = &counter,
  });
  EXPECT_EQ(2, counter.file_count);
  EXPECT_EQ(1, counter.cmd_count);

  counter.file_count = 0;
  counter.cmd_count = 0;
  spn_bg_bfs((spn_bg_it_config_t){
    .graph = uf->graphs.short_linear.graph,
    .direction = SPN_BG_ITER_DIR_OUT_TO_IN,
    .on_cmd = count_cmd_fn,
    .on_file = count_file_fn,
    .user_data = &counter,
  });
  EXPECT_EQ(2, counter.file_count);
  EXPECT_EQ(1, counter.cmd_count);

  counter.file_count = 0;
  counter.cmd_count = 0;
  spn_bg_dfs((spn_bg_it_config_t){
    .graph = uf->graphs.short_linear.graph,
    .direction = SPN_BG_ITER_DIR_IN_TO_OUT,
    .on_cmd = count_cmd_fn,
    .on_file = count_file_fn,
    .user_data = &counter,
  });
  EXPECT_EQ(2, counter.file_count);
  EXPECT_EQ(1, counter.cmd_count);
}

UTEST_F(spn_test_traversal, find_outputs) {
  EXPECT_EQ(sp_da_size(spn_bg_find_outputs(uf->graphs.short_linear.graph)), 1);
  EXPECT_EQ(sp_da_size(spn_bg_find_outputs(uf->graphs.fork_join.graph)), 1);
  EXPECT_EQ(sp_da_size(spn_bg_find_outputs(uf->graphs.split_join)), 1);
}



///////////
// DIRTY //
///////////
#define SPN_DIRTY_TEST_MAX_NODE 16

typedef struct {
  spn_bg_id_t dirty [SPN_DIRTY_TEST_MAX_NODE];
  spn_bg_id_t clean [SPN_DIRTY_TEST_MAX_NODE];
} expected_dirty_state_t;

typedef struct {
  expected_dirty_state_t files;
  expected_dirty_state_t commands;
  spn_bg_err_kind_t errors [SPN_DIRTY_TEST_MAX_NODE];
} expected_dirty_t;

void expect_dirty(s32* utest_result, spn_bg_dirty_t* dirty, expected_dirty_t ex) {
  sp_carr_for(ex.errors, it) {
    if (ex.errors[it] == SPN_BG_OK) break;

    EXPECT_TRUE(sp_da_size(dirty->errors) > it);
    EXPECT_TRUE(ex.errors[it] == dirty->errors[it].kind);
  }

  sp_carr_for(ex.files.dirty, it) {
    spn_bg_id_t id = ex.files.dirty[it];
    if (!id.occupied) break;

    EXPECT_TRUE(spn_bg_is_file_dirty(dirty, id));
  }

  sp_carr_for(ex.files.clean, it) {
    spn_bg_id_t id = ex.files.clean[it];
    if (!id.occupied) break;

    EXPECT_TRUE(!spn_bg_is_file_dirty(dirty, id));
  }

  sp_carr_for(ex.commands.dirty, it) {
    spn_bg_id_t id = ex.commands.dirty[it];
    if (!id.occupied) break;

    EXPECT_TRUE(spn_bg_is_cmd_dirty(dirty, id));
  }

  sp_carr_for(ex.commands.clean, it) {
    spn_bg_id_t id = ex.commands.clean[it];
    if (!id.occupied) break;

    EXPECT_TRUE(!spn_bg_is_cmd_dirty(dirty, id));
  }
}

struct spn_dirty_tests {
  sp_test_file_manager_t fm;
  graphs_t g;
};

UTEST_F_SETUP(spn_dirty_tests) {
  sp_test_file_manager_init(&uf->fm);
  uf->g = build_graphs(&uf->fm);
}

UTEST_F_TEARDOWN(spn_dirty_tests) {
  sp_test_file_manager_cleanup(&uf->fm);
}

UTEST_F(spn_dirty_tests, missing_input_errors) {
  spn_bg_dirty_t* dirty = spn_bg_compute_dirty(uf->g.short_linear.graph);

  expect_dirty(utest_result, dirty, (expected_dirty_t) {
    .errors = {
      SPN_BG_ERR_MISSING_INPUT
    },
  });
}

UTEST_F(spn_dirty_tests, missing_output_is_dirty) {
  short_linear_graph_t g = uf->g.short_linear;
  touch_node(g.graph, g.a);
  spn_bg_dirty_t* dirty = spn_bg_compute_dirty(g.graph);

  expect_dirty(utest_result, dirty, (expected_dirty_t) {
    .files = {
      .dirty = { g.b },
      .clean = { g.a },
    },
    .commands = {
      .dirty = { g.touch }
    }
  });
}

UTEST_F(spn_dirty_tests, input_newer_than_output_is_dirty) {
  short_linear_graph_t g = uf->g.short_linear;
  touch_node(g.graph, g.b);
  touch_node(g.graph, g.a);
  spn_bg_dirty_t* dirty = spn_bg_compute_dirty(g.graph);

  expect_dirty(utest_result, dirty, (expected_dirty_t) {
    .files = {
      .dirty = { g.b },
      .clean = { g.a },
    },
    .commands = {
      .dirty = { g.touch }
    }
  });
}

UTEST_F(spn_dirty_tests, output_newer_than_input_is_clean) {
  short_linear_graph_t g = uf->g.short_linear;
  touch_node(g.graph, g.a);
  touch_node(g.graph, g.b);
  spn_bg_dirty_t* dirty = spn_bg_compute_dirty(g.graph);

  expect_dirty(utest_result, dirty, (expected_dirty_t) {
    .files = {
      .clean = { g.b, g.a },
    },
    .commands = {
      .clean = { g.touch }
    }
  });
}

UTEST_F(spn_dirty_tests, long_linear_dirty_propagates) {
  long_linear_graph_t g = uf->g.long_linear;

  touch_node(g.graph, g.b);
  touch_node(g.graph, g.c);
  touch_node(g.graph, g.a);

  spn_bg_dirty_t* dirty = spn_bg_compute_dirty(g.graph);

  expect_dirty(utest_result, dirty, (expected_dirty_t) {
    .files = {
      .dirty = { g.b, g.c },
      .clean = { g.a },
    },
    .commands = {
      .dirty = { g.cmd1, g.cmd2 },
    },
  });
}

UTEST_F(spn_dirty_tests, long_linear_partial) {
  long_linear_graph_t g = uf->g.long_linear;

  // touch_node(g.graph, g.a);
  // touch_node(g.graph, g.c);
  // touch_node(g.graph, g.b);
  touch_node(g.graph, g.a);
  touch_node(g.graph, g.b);
  touch_node(g.graph, g.c);
  touch_node(g.graph, g.b);

  spn_bg_dirty_t* dirty = spn_bg_compute_dirty(g.graph);

  expect_dirty(utest_result, dirty, (expected_dirty_t) {
    .files = {
      .dirty = { g.c },
      .clean = { g.a, g.b },
    },
    .commands = {
      .clean = { g.cmd1 },
      .dirty = { g.cmd2 },
    },
  });
}

UTEST_F(spn_dirty_tests, diamond_propagation) {
  diamond_graph_t g = uf->g.diamond;
  touch_node(g.graph, g.a);

  spn_bg_dirty_t* dirty = spn_bg_compute_dirty(g.graph);

  expect_dirty(utest_result, dirty, (expected_dirty_t) {
    .files = {
      .dirty = { g.b, g.c, g.d },
      .clean = { g.a },
    },
    .commands = {
      .dirty = { g.left, g.right, g.join },
    },
  });
}

UTEST_F(spn_dirty_tests, fork_join_partial_rebuild) {
  fork_join_graph_t g = uf->g.fork_join;

  touch_node(g.graph, g.src2);
  touch_node(g.graph, g.obj2);
  touch_node(g.graph, g.exe);
  touch_node(g.graph, g.src1);

  spn_bg_dirty_t* dirty = spn_bg_compute_dirty(g.graph);

  expect_dirty(utest_result, dirty, (expected_dirty_t) {
    .files = {
      .dirty = { g.obj1, g.exe },
      .clean = { g.src1, g.src2, g.obj2 },
    },
    .commands = {
      .dirty = { g.compile1, g.link },
      .clean = { g.compile2 },
    },
  });
}

UTEST_F(spn_dirty_tests, multi_output_missing_peer) {
  multi_output_graph_t g = uf->g.multi_output;

  touch_node(g.graph, g.a);
  touch_node(g.graph, g.c);

  spn_bg_dirty_t* dirty = spn_bg_compute_dirty(g.graph);

  expect_dirty(utest_result, dirty, (expected_dirty_t) {
    .files = {
      .dirty = { g.b, g.c },
      .clean = { g.a },
    },
    .commands = {
      .dirty = { g.split },
    },
  });
}




//////////////
// EXECUTOR //
//////////////
typedef struct {
  spn_bg_id_t expected[SPN_DIRTY_TEST_MAX_NODE];
} expected_execution_t;

void
expect_execution(
  s32*                 utest_result,
  spn_build_graph_t*   graph,
  u32                  num_threads,
  expected_execution_t exp
) {
  spn_bg_dirty_t* dirty = spn_bg_compute_dirty(graph);
  spn_bg_executor_t* ex = spn_bg_executor_new(graph, dirty, num_threads);
  spn_bg_executor_run(ex);
  spn_bg_executor_join(ex);

  u32 num_expected = 0;
  sp_carr_for(exp.expected, it) {
    spn_bg_id_t id = exp.expected[it];
    if (!id.occupied) {
      break;
    }

    num_expected++;
    EXPECT_TRUE(sp_ht_key_exists(ex->completed, id));
  }

  u32 num_run = sp_da_size(ex->ran);
  EXPECT_EQ(num_run, num_expected);
}

typedef struct spn_executor_test {
  sp_test_file_manager_t file_manager;
  graphs_t graphs;
} spn_executor_test_t;

UTEST_F_SETUP(spn_executor_test) {
  sp_test_file_manager_init(&uf->file_manager);
  uf->graphs = build_graphs(&uf->file_manager);
}

UTEST_F_TEARDOWN(spn_executor_test) {
  sp_test_file_manager_cleanup(&uf->file_manager);
}

UTEST_F(spn_executor_test, short_linear_new_input) {
  short_linear_graph_t g = uf->graphs.short_linear;
  touch_node(g.graph, g.a);

  expect_execution(utest_result, g.graph, 4, (expected_execution_t) {
    .expected = { g.touch }
  });
}

UTEST_F(spn_executor_test, short_linear_clean) {
  short_linear_graph_t g = uf->graphs.short_linear;
  touch_node(g.graph, g.a);
  touch_node(g.graph, g.b);

  expect_execution(utest_result, g.graph, 4, SP_ZERO_STRUCT(expected_execution_t));
}

UTEST_F(spn_executor_test, long_linear_chain_propagation) {
  long_linear_graph_t g = uf->graphs.long_linear;
  touch_node(g.graph, g.a);

  expect_execution(utest_result, g.graph, 4, (expected_execution_t) {
    .expected = { g.cmd1, g.cmd2 }
  });
}

UTEST_F(spn_executor_test, long_linear_partial_dirty) {
  long_linear_graph_t g = uf->graphs.long_linear;
  touch_node(g.graph, g.a);
  touch_node(g.graph, g.b);
  touch_node(g.graph, g.c);
  // now touch b to make it newer than c
  touch_node(g.graph, g.b);

  expect_execution(utest_result, g.graph, 4, (expected_execution_t) {
    .expected = { g.cmd2 }
  });
}

UTEST_F(spn_executor_test, diamond_all_dirty) {
  diamond_graph_t g = uf->graphs.diamond;
  touch_node(g.graph, g.a);

  expect_execution(utest_result, g.graph, 4, (expected_execution_t) {
    .expected = { g.left, g.right, g.join }
  });
}

UTEST_F(spn_executor_test, fork_join_partial_dirty) {
  fork_join_graph_t g = uf->graphs.fork_join;
  // set up: src2, obj2 are up to date; src1 is newer than obj1
  touch_node(g.graph, g.src2);
  touch_node(g.graph, g.obj2);
  touch_node(g.graph, g.exe);
  touch_node(g.graph, g.src1);

  expect_execution(utest_result, g.graph, 4, (expected_execution_t) {
    .expected = { g.compile1, g.link }
  });
}

UTEST_F(spn_executor_test, multi_output_all_dirty) {
  multi_output_graph_t g = uf->graphs.multi_output;
  touch_node(g.graph, g.a);

  expect_execution(utest_result, g.graph, 4, (expected_execution_t) {
    .expected = { g.split, g.proc_b, g.proc_c }
  });
}

UTEST_MAIN();
