#define SP_IMPLEMENTATION
#include "sp.h"
#include "graph.h"

#define SP_TEST_IMPLEMENTATION
#include "test.h"
#include "utest.h"


void touch_file(sp_str_t path) {
  sp_io_stream_t s = sp_io_from_file(path, SP_IO_MODE_APPEND);
  sp_io_write_str(&s, sp_str_lit(" "));
  sp_io_close(&s);
}

#define uf utest_fixture

 // Graph A (simple):
 // ┌──────────┐     ┌─────┐     ┌────────────────┐     ┌────┐     ┌────────────┐
 // │ source.c │────▶│ gcc │────▶│ intermediate.o │────▶│ ld │────▶│ output.txt │
 // └──────────┘     └─────┘     └────────────────┘     └────┘     └────────────┘
spn_build_graph_t* create_linear_graph() {
  spn_build_graph_t* graph = spn_bg_new();
  spn_build_cmd_t* command = SP_NULLPTR;

  spn_bg_id_t output = spn_bg_add_file(graph, sp_str_lit("output.txt"));
  spn_bg_id_t intermediate = spn_bg_add_file(graph, sp_str_lit("intermediate.o"));
  spn_bg_id_t source = spn_bg_add_file(graph, sp_str_lit("source.c"));

  spn_bg_id_t link = spn_bg_add_command(graph, SPN_BUILD_CMD_SUBPROCESS);
  spn_bg_id_t compile = spn_bg_add_command(graph, SPN_BUILD_CMD_SUBPROCESS);

  command = spn_bg_find_command(graph, link);
  command->ps.command = sp_str_lit("ld");

  command = spn_bg_find_command(graph, compile);
  command->ps.command = sp_str_lit("gcc");

  spn_build_file_set_command(graph, output, link);
  spn_build_command_add_input(graph, link, intermediate);

  spn_build_file_set_command(graph, intermediate, compile);
  spn_build_command_add_input(graph, compile, source);

  return graph;
}

 // ┌────────┐     ┌─────┐     ┌────────┐
 // │ main.c │────▶│ gcc │────▶│ main.o │──┐
 // └────────┘     └─────┘     └────────┘  │     ┌────┐     ┌─────────────┐
 //                                        ├────▶│ ld │────▶│ program.exe │
 // ┌─────────┐     ┌─────┐     ┌─────────┐│     └────┘     └─────────────┘
 // │ utils.c │────▶│ gcc │────▶│ utils.o │┘
 // └─────────┘     └─────┘     └─────────┘
spn_build_graph_t* create_fork_join_graph() {
  spn_build_graph_t* graph = spn_bg_new();

  spn_bg_id_t exe = spn_bg_add_file(graph, sp_str_lit("program.exe"));
  spn_bg_id_t obj1 = spn_bg_add_file(graph, sp_str_lit("main.o"));
  spn_bg_id_t obj2 = spn_bg_add_file(graph, sp_str_lit("utils.o"));
  spn_bg_id_t src1 = spn_bg_add_file(graph, sp_str_lit("main.c"));
  spn_bg_id_t src2 = spn_bg_add_file(graph, sp_str_lit("utils.c"));

  spn_bg_id_t link = spn_bg_add_command(graph, SPN_BUILD_CMD_SUBPROCESS);
  spn_bg_id_t compile1 = spn_bg_add_command(graph, SPN_BUILD_CMD_SUBPROCESS);
  spn_bg_id_t compile2 = spn_bg_add_command(graph, SPN_BUILD_CMD_SUBPROCESS);

  spn_build_cmd_t* command = spn_bg_find_command(graph, link);
  command->ps.command = sp_str_lit("ld");

  command = spn_bg_find_command(graph, compile1);
  command->ps.command = sp_str_lit("gcc");

  command = spn_bg_find_command(graph, compile2);
  command->ps.command = sp_str_lit("gcc");

  spn_build_file_set_command(graph, exe, link);
  spn_build_command_add_input(graph, link, obj1);
  spn_build_command_add_input(graph, link, obj2);

  spn_build_file_set_command(graph, obj1, compile1);
  spn_build_command_add_input(graph, compile1, src1);

  spn_build_file_set_command(graph, obj2, compile2);
  spn_build_command_add_input(graph, compile2, src2);

  return graph;
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

  spn_bg_id_t split = spn_bg_add_command(g, SPN_BUILD_CMD_SUBPROCESS);
  spn_bg_id_t join = spn_bg_add_command(g, SPN_BUILD_CMD_SUBPROCESS);
  spn_bg_id_t love = spn_bg_add_command(g, SPN_BUILD_CMD_SUBPROCESS);

  spn_build_cmd_t* cmd = spn_bg_find_command(g, split);
  cmd->ps.command = sp_str_lit("split");

  cmd = spn_bg_find_command(g, join);
  cmd->ps.command = sp_str_lit("join");

  cmd = spn_bg_find_command(g, love);
  cmd->ps.command = sp_str_lit("love");

  spn_build_command_add_input(g, split, tdns);
  spn_build_file_set_command(g, td, split);
  spn_build_file_set_command(g, ns, split);

  spn_build_command_add_input(g, join, td);
  spn_build_command_add_input(g, join, ns);
  spn_build_file_set_command(g, tdns2, join);

  spn_build_command_add_input(g, love, tdns2);
  spn_build_command_add_input(g, love, alg);
  spn_build_file_set_command(g, tdns_loves_alg, love);

  return g;
}

spn_build_graph_t* create_single_graph() {
  spn_build_graph_t* g = spn_bg_new();
  spn_bg_id_t src = spn_bg_add_file(g, sp_str_lit("src"));
  spn_bg_id_t out = spn_bg_add_file(g, sp_str_lit("out"));
  spn_bg_id_t cmd = spn_bg_add_command(g, SPN_BUILD_CMD_SUBPROCESS);
  spn_bg_find_command(g, cmd)->ps.command = sp_str_lit("build");
  spn_build_command_add_input(g, cmd, src);
  spn_build_file_set_command(g, out, cmd);
  return g;
}

spn_build_graph_t* create_wide_parallel_graph() {
  spn_build_graph_t* g = spn_bg_new();
  for (u32 i = 0; i < 4; i++) {
    spn_bg_id_t src = spn_bg_add_file(g, sp_format("src{}", SP_FMT_U32(i)));
    spn_bg_id_t out = spn_bg_add_file(g, sp_format("out{}", SP_FMT_U32(i)));
    spn_bg_id_t cmd = spn_bg_add_command(g, SPN_BUILD_CMD_SUBPROCESS);
    spn_bg_find_command(g, cmd)->ps.command = sp_format("cmd{}", SP_FMT_U32(i));
    spn_build_command_add_input(g, cmd, src);
    spn_build_file_set_command(g, out, cmd);
  }
  return g;
}

spn_build_graph_t* create_diamond_graph() {
  spn_build_graph_t* g = spn_bg_new();
  spn_bg_id_t a = spn_bg_add_file(g, sp_str_lit("a"));
  spn_bg_id_t b = spn_bg_add_file(g, sp_str_lit("b"));
  spn_bg_id_t c = spn_bg_add_file(g, sp_str_lit("c"));
  spn_bg_id_t d = spn_bg_add_file(g, sp_str_lit("d"));

  spn_bg_id_t cmd1 = spn_bg_add_command(g, SPN_BUILD_CMD_SUBPROCESS);
  spn_bg_id_t cmd2 = spn_bg_add_command(g, SPN_BUILD_CMD_SUBPROCESS);
  spn_bg_id_t cmd3 = spn_bg_add_command(g, SPN_BUILD_CMD_SUBPROCESS);

  spn_bg_find_command(g, cmd1)->ps.command = sp_str_lit("left");
  spn_bg_find_command(g, cmd2)->ps.command = sp_str_lit("right");
  spn_bg_find_command(g, cmd3)->ps.command = sp_str_lit("join");

  spn_build_command_add_input(g, cmd1, a);
  spn_build_file_set_command(g, b, cmd1);

  spn_build_command_add_input(g, cmd2, a);
  spn_build_file_set_command(g, c, cmd2);

  spn_build_command_add_input(g, cmd3, b);
  spn_build_command_add_input(g, cmd3, c);
  spn_build_file_set_command(g, d, cmd3);

  return g;
}

spn_build_graph_t* create_multi_output_graph() {
  spn_build_graph_t* g = spn_bg_new();
  spn_bg_id_t a = spn_bg_add_file(g, sp_str_lit("a"));
  spn_bg_id_t b = spn_bg_add_file(g, sp_str_lit("b"));
  spn_bg_id_t c = spn_bg_add_file(g, sp_str_lit("c"));
  spn_bg_id_t d = spn_bg_add_file(g, sp_str_lit("d"));
  spn_bg_id_t e = spn_bg_add_file(g, sp_str_lit("e"));

  spn_bg_id_t cmd1 = spn_bg_add_command(g, SPN_BUILD_CMD_SUBPROCESS);
  spn_bg_id_t cmd2 = spn_bg_add_command(g, SPN_BUILD_CMD_SUBPROCESS);
  spn_bg_id_t cmd3 = spn_bg_add_command(g, SPN_BUILD_CMD_SUBPROCESS);

  spn_bg_find_command(g, cmd1)->ps.command = sp_str_lit("split");
  spn_bg_find_command(g, cmd2)->ps.command = sp_str_lit("proc_b");
  spn_bg_find_command(g, cmd3)->ps.command = sp_str_lit("proc_c");

  spn_build_command_add_input(g, cmd1, a);
  spn_build_file_set_command(g, b, cmd1);
  spn_build_file_set_command(g, c, cmd1);

  spn_build_command_add_input(g, cmd2, b);
  spn_build_file_set_command(g, d, cmd2);

  spn_build_command_add_input(g, cmd3, c);
  spn_build_file_set_command(g, e, cmd3);

  return g;
}

spn_build_graph_t* create_error_chain_graph() {
  spn_build_graph_t* g = spn_bg_new();
  spn_bg_id_t a = spn_bg_add_file(g, sp_str_lit("a"));
  spn_bg_id_t b = spn_bg_add_file(g, sp_str_lit("b"));
  spn_bg_id_t c = spn_bg_add_file(g, sp_str_lit("c"));

  spn_bg_id_t cmd1 = spn_bg_add_command(g, SPN_BUILD_CMD_SUBPROCESS);
  spn_bg_id_t cmd2 = spn_bg_add_command(g, SPN_BUILD_CMD_SUBPROCESS);

  spn_bg_find_command(g, cmd1)->ps.command = sp_str_lit("fail");
  spn_bg_find_command(g, cmd2)->ps.command = sp_str_lit("after");

  spn_build_command_add_input(g, cmd1, a);
  spn_build_file_set_command(g, b, cmd1);

  spn_build_command_add_input(g, cmd2, b);
  spn_build_file_set_command(g, c, cmd2);

  return g;
}

typedef struct {
  spn_build_graph_t* linear;
  spn_build_graph_t* fork_join;
  spn_build_graph_t* split_join;
  spn_build_graph_t* single;
  spn_build_graph_t* wide_parallel;
  spn_build_graph_t* diamond;
  spn_build_graph_t* multi_output;
  spn_build_graph_t* error_chain;
} spn_test_graphs_t;

struct spn_test_graphs {
  spn_test_graphs_t graphs;
};

UTEST_F_SETUP(spn_test_graphs) {
  uf->graphs.linear = create_linear_graph();
  uf->graphs.fork_join = create_fork_join_graph();
  uf->graphs.split_join = create_split_join_graph();
  uf->graphs.single = create_single_graph();
  uf->graphs.wide_parallel = create_wide_parallel_graph();
  uf->graphs.diamond = create_diamond_graph();
  uf->graphs.multi_output = create_multi_output_graph();
  uf->graphs.error_chain = create_error_chain_graph();
}

UTEST_F_TEARDOWN(spn_test_graphs) {
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

UTEST_F(spn_test_graphs, visit_once) {
  visit_counter_t counter = {0};

  spn_bg_dfs((spn_bg_it_config_t){
    .graph = uf->graphs.linear,
    .direction = SPN_BG_ITER_DIR_OUT_TO_IN,
    .on_cmd = count_cmd_fn,
    .on_file = count_file_fn,
    .user_data = &counter,
  });
  EXPECT_EQ(3, counter.file_count);
  EXPECT_EQ(2, counter.cmd_count);

  counter.file_count = 0;
  counter.cmd_count = 0;
  spn_bg_bfs((spn_bg_it_config_t){
    .graph = uf->graphs.linear,
    .direction = SPN_BG_ITER_DIR_OUT_TO_IN,
    .on_cmd = count_cmd_fn,
    .on_file = count_file_fn,
    .user_data = &counter,
  });
  EXPECT_EQ(3, counter.file_count);
  EXPECT_EQ(2, counter.cmd_count);

  counter.file_count = 0;
  counter.cmd_count = 0;
  spn_bg_dfs((spn_bg_it_config_t){
    .graph = uf->graphs.linear,
    .direction = SPN_BG_ITER_DIR_IN_TO_OUT,
    .on_cmd = count_cmd_fn,
    .on_file = count_file_fn,
    .user_data = &counter,
  });
  EXPECT_EQ(3, counter.file_count);
  EXPECT_EQ(2, counter.cmd_count);
}

UTEST_F(spn_test_graphs, find_outputs) {
  sp_da(spn_bg_id_t) outputs_a = spn_bg_find_outputs(uf->graphs.linear);
  EXPECT_EQ(1, sp_da_size(outputs_a));

  sp_da(spn_bg_id_t) outputs_b = spn_bg_find_outputs(uf->graphs.fork_join);
  EXPECT_EQ(1, sp_da_size(outputs_b));

  sp_da(spn_bg_id_t) outputs_c = spn_bg_find_outputs(uf->graphs.split_join);
  EXPECT_EQ(1, sp_da_size(outputs_c));
}

struct spn_dirty_tests {
  sp_test_file_manager_t fm;
};

UTEST_F_SETUP(spn_dirty_tests) {
  sp_test_file_manager_init(&uf->fm);
}

UTEST_F_TEARDOWN(spn_dirty_tests) {
  sp_test_file_manager_cleanup(&uf->fm);
}

typedef struct {
  spn_build_graph_t* g;
  spn_bg_id_t src;
  spn_bg_id_t out;
  spn_bg_id_t cmd;
} dirty_test_graph_t;

// ┌───────┐     ┌─────┐     ┌───────┐
// │  src  │────▶│ cmd │────▶│  out  │
// └───────┘     └─────┘     └───────┘
dirty_test_graph_t create_dirty_test_graph(sp_str_t src_path, sp_str_t out_path) {
  dirty_test_graph_t r;
  r.g = spn_bg_new();
  r.src = spn_bg_add_file(r.g, src_path);
  r.out = spn_bg_add_file(r.g, out_path);
  r.cmd = spn_bg_add_command(r.g, SPN_BUILD_CMD_SUBPROCESS);
  spn_build_command_add_input(r.g, r.cmd, r.src);
  spn_build_file_set_command(r.g, r.out, r.cmd);
  return r;
}

UTEST_F(spn_dirty_tests, missing_input_errors) {
  dirty_test_graph_t t = create_dirty_test_graph(sp_str_lit("nonexistent.c"), sp_str_lit("out.o"));
  spn_bg_dirty_t* dirty = spn_bg_compute_dirty(t.g);

  EXPECT_EQ(1, sp_da_size(dirty->errors));
  EXPECT_EQ(SPN_BG_ERR_MISSING_INPUT, dirty->errors[0].kind);
}

UTEST_F(spn_dirty_tests, missing_output_is_dirty) {
  sp_str_t src_path = sp_test_file_create_empty(&uf->fm, sp_str_lit("src.c"));
  dirty_test_graph_t t = create_dirty_test_graph(src_path, sp_str_lit("missing.o"));
  spn_bg_dirty_t* dirty = spn_bg_compute_dirty(t.g);

  EXPECT_EQ(0, sp_da_size(dirty->errors));
  EXPECT_TRUE(spn_bg_is_cmd_dirty(dirty, t.cmd));
  EXPECT_TRUE(spn_bg_is_file_dirty(dirty, t.out));
}

UTEST_F(spn_dirty_tests, input_newer_than_output_is_dirty) {
  sp_str_t src_path = sp_test_file_create_empty(&uf->fm, sp_str_lit("src.c"));
  sp_str_t out_path = sp_test_file_create_empty(&uf->fm, sp_str_lit("out.o"));
  sp_os_sleep_ms(10);
  touch_file(src_path);

  dirty_test_graph_t t = create_dirty_test_graph(src_path, out_path);
  spn_bg_dirty_t* dirty = spn_bg_compute_dirty(t.g);

  EXPECT_EQ(0, sp_da_size(dirty->errors));
  EXPECT_TRUE(spn_bg_is_cmd_dirty(dirty, t.cmd));
}

UTEST_F(spn_dirty_tests, output_newer_than_input_is_clean) {
  sp_str_t src_path = sp_test_file_create_empty(&uf->fm, sp_str_lit("src.c"));
  sp_os_sleep_ms(10);
  sp_str_t out_path = sp_test_file_create_empty(&uf->fm, sp_str_lit("out.o"));

  dirty_test_graph_t t = create_dirty_test_graph(src_path, out_path);
  spn_bg_dirty_t* dirty = spn_bg_compute_dirty(t.g);

  EXPECT_EQ(0, sp_da_size(dirty->errors));
  EXPECT_FALSE(spn_bg_is_cmd_dirty(dirty, t.cmd));
  EXPECT_FALSE(spn_bg_is_file_dirty(dirty, t.out));
}

// ┌─────┐     ┌──────┐     ┌─────┐     ┌──────┐     ┌───────┐
// │  a  │────▶│ cmd1 │────▶│  b  │────▶│ cmd2 │────▶│   c   │
// └─────┘     └──────┘     └─────┘     └──────┘     └───────┘
UTEST_F(spn_dirty_tests, dirty_propagates_through_chain) {
  sp_str_t a_path = sp_test_file_create_empty(&uf->fm, sp_str_lit("a.c"));
  sp_os_sleep_ms(10);
  sp_str_t b_path = sp_test_file_create_empty(&uf->fm, sp_str_lit("b.o"));
  sp_os_sleep_ms(10);
  sp_str_t c_path = sp_test_file_create_empty(&uf->fm, sp_str_lit("c.exe"));

  sp_os_sleep_ms(10);
  touch_file(a_path);

  spn_build_graph_t* g = spn_bg_new();
  spn_bg_id_t a = spn_bg_add_file(g, a_path);
  spn_bg_id_t b = spn_bg_add_file(g, b_path);
  spn_bg_id_t c = spn_bg_add_file(g, c_path);

  spn_bg_id_t cmd1 = spn_bg_add_command(g, SPN_BUILD_CMD_SUBPROCESS);
  spn_bg_id_t cmd2 = spn_bg_add_command(g, SPN_BUILD_CMD_SUBPROCESS);

  spn_build_command_add_input(g, cmd1, a);
  spn_build_file_set_command(g, b, cmd1);

  spn_build_command_add_input(g, cmd2, b);
  spn_build_file_set_command(g, c, cmd2);

  spn_bg_dirty_t* dirty = spn_bg_compute_dirty(g);

  EXPECT_EQ(0, sp_da_size(dirty->errors));
  EXPECT_TRUE(spn_bg_is_cmd_dirty(dirty, cmd1));  // a > b
  EXPECT_TRUE(spn_bg_is_file_dirty(dirty, b));     // produced by dirty cmd
  EXPECT_TRUE(spn_bg_is_cmd_dirty(dirty, cmd2));   // input b is dirty
  EXPECT_TRUE(spn_bg_is_file_dirty(dirty, c));     // produced by dirty cmd
}

// ============================================================================
// Scratchpad for executor experiments
// ============================================================================

sp_mutex_t g_log_mutex;

void log_cmd_execute(spn_build_cmd_t* cmd, void* user_data) {
  sp_mutex_lock(&g_log_mutex);
  printf("cmd %u: %.*s\n",
    cmd->id.index,
    (int)cmd->ps.command.len, cmd->ps.command.data);
  sp_mutex_unlock(&g_log_mutex);

  // Simulate some work
  sp_os_sleep_ms(10);
}

void set_all_cmds_on_execute(spn_build_graph_t* graph) {
  sp_da_for(graph->commands, i) {
    graph->commands[i].kind = SPN_BUILD_CMD_FN;
    graph->commands[i].fn.on_execute = log_cmd_execute;
  }
}

UTEST(scratchpad, executor_experiment) {
  sp_mutex_init(&g_log_mutex, SP_MUTEX_PLAIN);

  // Use graph C which has parallel paths
  spn_build_graph_t* g = create_split_join_graph();
  set_all_cmds_on_execute(g);

  // Mark all commands dirty (no files on disk)
  spn_bg_dirty_t* dirty = SP_ALLOC(spn_bg_dirty_t);
  sp_ht_set_fns(dirty->commands, sp_ht_on_hash_key, sp_ht_on_compare_key);
  sp_da_for(g->commands, i) {
    sp_ht_insert(dirty->commands, g->commands[i].id, true);
  }

  printf("\n--- Running executor with 4 threads ---\n");
  spn_bg_executor_t* ex = spn_bg_executor_new(g, dirty, 4);
  spn_bg_executor_run(ex);

  printf("--- Ran %u commands ---\n", sp_da_size(ex->ran));
  sp_da_for(ex->ran, i) {
    spn_build_cmd_t* cmd = spn_bg_find_command(g, ex->ran[i]);
    printf("  %u: %.*s\n", ex->ran[i].index, (int)cmd->ps.command.len, cmd->ps.command.data);
  }

  EXPECT_EQ(3, sp_da_size(ex->ran));
}

UTEST_MAIN();
