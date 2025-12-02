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
  spn_bg_file_t* file = spn_bg_find_file(graph, id);
  SP_ASSERT(file);
  touch_file(file->path);
}



///////////////////////
// BUILD COMMAND FNS //
///////////////////////
sp_mutex_t g_log_mutex;

void build_fn_noop(spn_bg_cmd_t* cmd, void* ud) {

}

#define uf utest_fixture



////////////
// GRAPHS //
////////////
// ┌───┐     ┌─────────┐     ┌───┐
// │ a │────▶│ compile │────▶│ b │
// └───┘     └─────────┘     └───┘
typedef struct {
  spn_build_graph_t* graph;
  spn_bg_id_t a;
  spn_bg_id_t b;
  spn_bg_id_t compile;
} short_linear_graph_t;

short_linear_graph_t create_short_linear_graph() {
  short_linear_graph_t g;
  g.graph = spn_bg_new();
  g.a = spn_bg_add_file(g.graph, sp_str_lit("a"));
  g.b = spn_bg_add_file(g.graph, sp_str_lit("b"));
  g.compile = spn_bg_add_command(g.graph, SPN_BUILD_CMD_FN);

  spn_bg_tag_command_c(g.graph, g.compile, "compile");
  spn_bg_cmd_add_input(g.graph, g.compile, g.a);
  spn_bg_cmd_add_output(g.graph, g.compile, g.b);

  return g;
}

// ┌───┐     ┌─────────┐     ┌───┐     ┌─────────┐     ┌───┐
// │ a │────▶│ compile │────▶│ b │────▶│ compile │────▶│ c │
// └───┘     └─────────┘     └───┘     └─────────┘     └───┘
typedef struct {
  spn_build_graph_t* graph;
  spn_bg_id_t a, b, c;
  spn_bg_id_t compile_1, compile_2;
} long_linear_graph_t;

long_linear_graph_t create_long_linear_graph() {
  long_linear_graph_t r;
  r.graph = spn_bg_new();
  r.a = spn_bg_add_file(r.graph, sp_str_lit("a"));
  r.b = spn_bg_add_file(r.graph, sp_str_lit("b"));
  r.c = spn_bg_add_file(r.graph, sp_str_lit("c"));

  r.compile_1 = spn_bg_add_command(r.graph, SPN_BUILD_CMD_FN);
  r.compile_2 = spn_bg_add_command(r.graph, SPN_BUILD_CMD_FN);

  spn_bg_tag_command_c(r.graph, r.compile_1, "compile");
  spn_bg_tag_command_c(r.graph, r.compile_2, "compile");

  spn_bg_cmd_add_input(r.graph, r.compile_1, r.a);
  spn_bg_file_set_producer(r.graph, r.b, r.compile_1);

  spn_bg_cmd_add_input(r.graph, r.compile_2, r.b);
  spn_bg_file_set_producer(r.graph, r.c, r.compile_2);

  return r;
}

 // ┌───┐     ┌─────────┐     ┌───┐
 // │ a │────▶│ compile │────▶│ b │──┐
 // └───┘     └─────────┘     └───┘  │  ┌──────┐     ┌───┐
 //                                  ├─▶│ join │────▶│ d │
 // ┌───┐     ┌─────────┐     ┌───┐  │  └──────┘     └───┘
 // │ c │────▶│ compile │────▶│ e │──┘
 // └───┘     └─────────┘     └───┘
typedef struct {
  spn_build_graph_t* graph;
  spn_bg_id_t a, b, c, d, e;
  spn_bg_id_t compile_1, compile_2, join;
} fork_join_graph_t;

fork_join_graph_t create_fork_join_graph() {
  fork_join_graph_t r;
  r.graph = spn_bg_new();

  r.a = spn_bg_add_file(r.graph, sp_str_lit("a"));
  r.b = spn_bg_add_file(r.graph, sp_str_lit("b"));
  r.c = spn_bg_add_file(r.graph, sp_str_lit("c"));
  r.d = spn_bg_add_file(r.graph, sp_str_lit("d"));
  r.e = spn_bg_add_file(r.graph, sp_str_lit("e"));

  r.join = spn_bg_add_command(r.graph, SPN_BUILD_CMD_FN);
  r.compile_1 = spn_bg_add_command(r.graph, SPN_BUILD_CMD_FN);
  r.compile_2 = spn_bg_add_command(r.graph, SPN_BUILD_CMD_FN);

  spn_bg_tag_command_c(r.graph, r.join, "join");
  spn_bg_tag_command_c(r.graph, r.compile_1, "compile");
  spn_bg_tag_command_c(r.graph, r.compile_2, "compile");

  spn_bg_cmd_add_input(r.graph, r.join, r.b);
  spn_bg_cmd_add_input(r.graph, r.join, r.e);
  spn_bg_cmd_add_output(r.graph, r.join, r.d);

  spn_bg_cmd_add_input(r.graph, r.compile_1, r.a);
  spn_bg_cmd_add_output(r.graph, r.compile_1, r.b);

  spn_bg_cmd_add_input(r.graph, r.compile_2, r.c);
  spn_bg_cmd_add_output(r.graph, r.compile_2, r.e);

  return r;
}

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
typedef struct {
  spn_build_graph_t* graph;
  spn_bg_id_t a, b, c, d, e, f;
  spn_bg_id_t split, join_1, join_2;
} split_join_graph_t;

split_join_graph_t create_split_join_graph() {
  split_join_graph_t r;
  r.graph = spn_bg_new();
  r.a = spn_bg_add_file(r.graph, sp_str_lit("a"));
  r.b = spn_bg_add_file(r.graph, sp_str_lit("b"));
  r.c = spn_bg_add_file(r.graph, sp_str_lit("c"));
  r.d = spn_bg_add_file(r.graph, sp_str_lit("d"));
  r.e = spn_bg_add_file(r.graph, sp_str_lit("e"));
  r.f = spn_bg_add_file(r.graph, sp_str_lit("f"));

  r.split = spn_bg_add_command(r.graph, SPN_BUILD_CMD_FN);
  r.join_1 = spn_bg_add_command(r.graph, SPN_BUILD_CMD_FN);
  r.join_2 = spn_bg_add_command(r.graph, SPN_BUILD_CMD_FN);

  spn_bg_tag_command_c(r.graph, r.split, "split");
  spn_bg_tag_command_c(r.graph, r.join_1, "join");
  spn_bg_tag_command_c(r.graph, r.join_2, "join");

  spn_bg_cmd_add_input(r.graph, r.split, r.a);
  spn_bg_cmd_add_output(r.graph, r.split, r.b);
  spn_bg_cmd_add_output(r.graph, r.split, r.c);

  spn_bg_cmd_add_input(r.graph, r.join_1, r.b);
  spn_bg_cmd_add_input(r.graph, r.join_1, r.c);
  spn_bg_file_set_producer(r.graph, r.d, r.join_1);

  spn_bg_cmd_add_input(r.graph, r.join_2, r.d);
  spn_bg_cmd_add_input(r.graph, r.join_2, r.e);
  spn_bg_file_set_producer(r.graph, r.f, r.join_2);

  return r;
}

//          ┌─────────┐     ┌───┐
//       ┌─▶│ compile │────▶│ b │──┐
// ┌───┐ │  └─────────┘     └───┘  │  ┌──────┐     ┌───┐
// │ a │─┤                         ├─▶│ join │────▶│ d │
// └───┘ │  ┌─────────┐     ┌───┐  │  └──────┘     └───┘
//       └─▶│ compile │────▶│ c │──┘
//          └─────────┘     └───┘
typedef struct {
  spn_build_graph_t* graph;
  spn_bg_id_t a, b, c, d;
  spn_bg_id_t compile_1, compile_2, join;
} diamond_graph_t;

diamond_graph_t create_diamond_graph() {
  diamond_graph_t r;
  r.graph = spn_bg_new();
  r.a = spn_bg_add_file(r.graph, sp_str_lit("a"));
  r.b = spn_bg_add_file(r.graph, sp_str_lit("b"));
  r.c = spn_bg_add_file(r.graph, sp_str_lit("c"));
  r.d = spn_bg_add_file(r.graph, sp_str_lit("d"));
  r.compile_1 = spn_bg_add_command(r.graph, SPN_BUILD_CMD_FN);
  r.compile_2 = spn_bg_add_command(r.graph, SPN_BUILD_CMD_FN);
  r.join = spn_bg_add_command(r.graph, SPN_BUILD_CMD_FN);

  spn_bg_tag_command_c(r.graph, r.compile_1, "compile");
  spn_bg_tag_command_c(r.graph, r.compile_2, "compile");
  spn_bg_tag_command_c(r.graph, r.join, "join");

  spn_bg_cmd_add_input(r.graph, r.compile_1, r.a);
  spn_bg_file_set_producer(r.graph, r.b, r.compile_1);

  spn_bg_cmd_add_input(r.graph, r.compile_2, r.a);
  spn_bg_file_set_producer(r.graph, r.c, r.compile_2);

  spn_bg_cmd_add_input(r.graph, r.join, r.b);
  spn_bg_cmd_add_input(r.graph, r.join, r.c);
  spn_bg_file_set_producer(r.graph, r.d, r.join);

  return r;
}

// ┌───┐     ┌─────────┐     ┌───┐     ┌─────────┐     ┌───┐     ┌─────────┐     ┌───┐     ┌─────────┐     ┌───┐
// │ a │────▶│ compile │────▶│ b │────▶│ compile │────▶│ c │────▶│ compile │────▶│ d │────▶│ compile │────▶│ e │──┐
// └───┘     └─────────┘     └───┘     └─────────┘     └───┘     └─────────┘     └───┘     └─────────┘     └───┘  │
//                                                                                                                │  ┌──────┐     ┌───┐
//                                                                                                                ├─▶│ join │────▶│ g │
//                                                                                                         ┌───┐  │  └──────┘     └───┘
//                                                                                                         │ f │──┘
//                                                                                                         └───┘
typedef struct {
  spn_build_graph_t* graph;
  spn_bg_id_t a, b, c, d, e, f, g;
  spn_bg_id_t compile_1, compile_2, compile_3, compile_4, join;
} asymmetric_fork_graph_t;

asymmetric_fork_graph_t create_asymmetric_fork_graph() {
  asymmetric_fork_graph_t r;
  r.graph = spn_bg_new();

  r.a = spn_bg_add_file(r.graph, sp_str_lit("a"));
  r.b = spn_bg_add_file(r.graph, sp_str_lit("b"));
  r.c = spn_bg_add_file(r.graph, sp_str_lit("c"));
  r.d = spn_bg_add_file(r.graph, sp_str_lit("d"));
  r.e = spn_bg_add_file(r.graph, sp_str_lit("e"));
  r.f = spn_bg_add_file(r.graph, sp_str_lit("f"));
  r.g = spn_bg_add_file(r.graph, sp_str_lit("g"));

  r.compile_1 = spn_bg_add_command(r.graph, SPN_BUILD_CMD_FN);
  r.compile_2 = spn_bg_add_command(r.graph, SPN_BUILD_CMD_FN);
  r.compile_3 = spn_bg_add_command(r.graph, SPN_BUILD_CMD_FN);
  r.compile_4 = spn_bg_add_command(r.graph, SPN_BUILD_CMD_FN);
  r.join = spn_bg_add_command(r.graph, SPN_BUILD_CMD_FN);

  spn_bg_tag_command_c(r.graph, r.compile_1, "compile");
  spn_bg_tag_command_c(r.graph, r.compile_2, "compile");
  spn_bg_tag_command_c(r.graph, r.compile_3, "compile");
  spn_bg_tag_command_c(r.graph, r.compile_4, "compile");
  spn_bg_tag_command_c(r.graph, r.join, "join");

  // Long top chain: a -> compile_1 -> b -> compile_2 -> c -> compile_3 -> d -> compile_4 -> e
  spn_bg_cmd_add_input(r.graph, r.compile_1, r.a);
  spn_bg_file_set_producer(r.graph, r.b, r.compile_1);

  spn_bg_cmd_add_input(r.graph, r.compile_2, r.b);
  spn_bg_file_set_producer(r.graph, r.c, r.compile_2);

  spn_bg_cmd_add_input(r.graph, r.compile_3, r.c);
  spn_bg_file_set_producer(r.graph, r.d, r.compile_3);

  spn_bg_cmd_add_input(r.graph, r.compile_4, r.d);
  spn_bg_file_set_producer(r.graph, r.e, r.compile_4);

  // Join: e + f -> join -> g
  spn_bg_cmd_add_input(r.graph, r.join, r.e);
  spn_bg_cmd_add_input(r.graph, r.join, r.f);
  spn_bg_file_set_producer(r.graph, r.g, r.join);

  return r;
}

//                       ┌───┐     ┌─────────┐     ┌───┐
//                    ┌─▶│ b │────▶│ compile │────▶│ d │
// ┌───┐     ┌───────┐│  └───┘     └─────────┘     └───┘
// │ a │────▶│ split │┤
// └───┘     └───────┘│  ┌───┐     ┌─────────┐     ┌───┐
//                    └─▶│ c │────▶│ compile │────▶│ e │
//                       └───┘     └─────────┘     └───┘
typedef struct {
  spn_build_graph_t* graph;
  spn_bg_id_t a, b, c, d, e;
  spn_bg_id_t split, compile_1, compile_2;
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
  r.compile_1 = spn_bg_add_command(r.graph, SPN_BUILD_CMD_FN);
  r.compile_2 = spn_bg_add_command(r.graph, SPN_BUILD_CMD_FN);

  spn_bg_tag_command_c(r.graph, r.split, "split");
  spn_bg_tag_command_c(r.graph, r.compile_1, "compile");
  spn_bg_tag_command_c(r.graph, r.compile_2, "compile");

  spn_bg_cmd_add_input(r.graph, r.split, r.a);
  spn_bg_file_set_producer(r.graph, r.b, r.split);
  spn_bg_file_set_producer(r.graph, r.c, r.split);

  spn_bg_cmd_add_input(r.graph, r.compile_1, r.b);
  spn_bg_file_set_producer(r.graph, r.d, r.compile_1);

  spn_bg_cmd_add_input(r.graph, r.compile_2, r.c);
  spn_bg_file_set_producer(r.graph, r.e, r.compile_2);

  return r;
}

// ┌───┐
// │ a │─┬──────────────────────┬───────────────────────┐
// └───┘ │                      │                       │
//       ▼                      ▼                       ▼
//   ┌─────────┐     ┌───┐  ┌─────────┐     ┌───┐  ┌─────────┐     ┌───┐
//   │ compile │────▶│ b │─▶│ compile │────▶│ c │─▶│ compile │────▶│ d │
//   └─────────┘     └───┘  └─────────┘     └───┘  └─────────┘     └───┘
typedef struct {
  spn_build_graph_t* graph;
  spn_bg_id_t a, b, c, d;
  spn_bg_id_t compile_1, compile_2, compile_3;
} comb_graph_t;

comb_graph_t create_comb_graph() {
  comb_graph_t r;
  r.graph = spn_bg_new();

  r.a = spn_bg_add_file(r.graph, sp_str_lit("a"));
  r.b = spn_bg_add_file(r.graph, sp_str_lit("b"));
  r.c = spn_bg_add_file(r.graph, sp_str_lit("c"));
  r.d = spn_bg_add_file(r.graph, sp_str_lit("d"));

  r.compile_1 = spn_bg_add_command(r.graph, SPN_BUILD_CMD_FN);
  r.compile_2 = spn_bg_add_command(r.graph, SPN_BUILD_CMD_FN);
  r.compile_3 = spn_bg_add_command(r.graph, SPN_BUILD_CMD_FN);

  spn_bg_tag_command_c(r.graph, r.compile_1, "compile");
  spn_bg_tag_command_c(r.graph, r.compile_2, "compile");
  spn_bg_tag_command_c(r.graph, r.compile_3, "compile");

  // a -> compile_1 -> b
  spn_bg_cmd_add_input(r.graph, r.compile_1, r.a);
  spn_bg_file_set_producer(r.graph, r.b, r.compile_1);

  // b + a -> compile_2 -> c
  spn_bg_cmd_add_input(r.graph, r.compile_2, r.b);
  spn_bg_cmd_add_input(r.graph, r.compile_2, r.a);
  spn_bg_file_set_producer(r.graph, r.c, r.compile_2);

  // c + a -> compile_3 -> d
  spn_bg_cmd_add_input(r.graph, r.compile_3, r.c);
  spn_bg_cmd_add_input(r.graph, r.compile_3, r.a);
  spn_bg_file_set_producer(r.graph, r.d, r.compile_3);

  return r;
}

// ┌─────────┐     ┌───┐     ┌─────────┐     ┌───┐
// │ generate│────▶│ a │────▶│ compile │────▶│ b │
// └─────────┘     └───┘     └─────────┘     └───┘
typedef struct {
  spn_build_graph_t* graph;
  spn_bg_id_t a, b;
  spn_bg_id_t generate, compile;
} no_input_graph_t;

no_input_graph_t create_no_input_graph() {
  no_input_graph_t r;
  r.graph = spn_bg_new();

  r.a = spn_bg_add_file(r.graph, sp_str_lit("a"));
  r.b = spn_bg_add_file(r.graph, sp_str_lit("b"));

  r.generate = spn_bg_add_command(r.graph, SPN_BUILD_CMD_FN);
  r.compile = spn_bg_add_command(r.graph, SPN_BUILD_CMD_FN);

  spn_bg_tag_command_c(r.graph, r.generate, "generate");
  spn_bg_tag_command_c(r.graph, r.compile, "compile");

  // generate (no inputs) -> a
  spn_bg_file_set_producer(r.graph, r.a, r.generate);

  // a -> compile -> b
  spn_bg_cmd_add_input(r.graph, r.compile, r.a);
  spn_bg_file_set_producer(r.graph, r.b, r.compile);

  return r;
}

typedef struct {
  short_linear_graph_t short_linear;
  fork_join_graph_t fork_join;
  split_join_graph_t split_join;
  long_linear_graph_t long_linear;
  diamond_graph_t diamond;
  multi_output_graph_t multi_output;
  asymmetric_fork_graph_t asymmetric_fork;
  comb_graph_t comb;
  no_input_graph_t no_input;
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
    .asymmetric_fork = create_asymmetric_fork_graph(),
    .comb = create_comb_graph(),
    .no_input = create_no_input_graph(),
  };

  bind_graph(graphs.short_linear.graph, file_manager);
  bind_graph(graphs.fork_join.graph, file_manager);
  bind_graph(graphs.split_join.graph, file_manager);
  bind_graph(graphs.long_linear.graph, file_manager);
  bind_graph(graphs.diamond.graph, file_manager);
  bind_graph(graphs.multi_output.graph, file_manager);
  bind_graph(graphs.asymmetric_fork.graph, file_manager);
  bind_graph(graphs.comb.graph, file_manager);
  bind_graph(graphs.no_input.graph, file_manager);

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

void count_file_fn(spn_build_graph_t* graph, spn_bg_file_t* file, void* user_data) {
  visit_counter_t* counter = (visit_counter_t*)user_data;
  counter->file_count++;
}

void count_cmd_fn(spn_build_graph_t* graph, spn_bg_cmd_t* cmd, void* user_data) {
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
  EXPECT_EQ(sp_da_size(spn_bg_find_outputs(uf->graphs.split_join.graph)), 1);
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

void expect_dirty(s32* utest_result, spn_build_graph_t* graph, spn_bg_dirty_t* dirty, expected_dirty_t ex) {
  sp_carr_for(ex.errors, it) {
    if (ex.errors[it] == SPN_BG_OK) break;

    EXPECT_TRUE(sp_da_size(dirty->errors) > it);
    EXPECT_TRUE(ex.errors[it] == dirty->errors[it].kind);
  }

  sp_carr_for(ex.files.dirty, it) {
    spn_bg_id_t id = ex.files.dirty[it];
    if (!id.occupied) break;

    bool is_dirty = spn_bg_is_file_dirty(dirty, id);
    if (!is_dirty) {
      sp_str_t message = sp_format("{:fg cyan} was not dirty", SP_FMT_STR(spn_bg_file_id_to_str(graph, id)));
      EXPECT_TRUE_MSG(is_dirty, sp_str_to_cstr(message));
    }
  }

  sp_carr_for(ex.files.clean, it) {
    spn_bg_id_t id = ex.files.clean[it];
    if (!id.occupied) break;

    bool is_dirty = spn_bg_is_file_dirty(dirty, id);
    if (is_dirty) {
      sp_str_t message = sp_format("{:fg cyan} was not clean", SP_FMT_STR(spn_bg_file_id_to_str(graph, id)));
      EXPECT_FALSE_MSG(is_dirty, sp_str_to_cstr(message));
    }
  }

  sp_carr_for(ex.commands.dirty, it) {
    spn_bg_id_t id = ex.commands.dirty[it];
    if (!id.occupied) break;

    bool is_dirty = spn_bg_is_cmd_dirty(dirty, id);
    if (!is_dirty) {
      sp_str_t message = sp_format("{:fg yellow} was not dirty", SP_FMT_STR(spn_bg_cmd_id_to_str(graph, id)));
      EXPECT_TRUE_MSG(is_dirty, sp_str_to_cstr(message));
    }
  }

  sp_carr_for(ex.commands.clean, it) {
    spn_bg_id_t id = ex.commands.clean[it];
    if (!id.occupied) break;

    bool is_dirty = spn_bg_is_cmd_dirty(dirty, id);
    if (is_dirty) {
      sp_str_t message = sp_format("{:fg yellow} was not clean", SP_FMT_STR(spn_bg_cmd_id_to_str(graph, id)));
      EXPECT_FALSE_MSG(is_dirty, sp_str_to_cstr(message));
    }
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
  spn_build_graph_t* graph = uf->g.short_linear.graph;
  spn_bg_dirty_t* dirty = spn_bg_compute_dirty(graph);

  expect_dirty(utest_result, graph, dirty, (expected_dirty_t) {
    .errors = {
      SPN_BG_ERR_MISSING_INPUT
    },
  });
}

UTEST_F(spn_dirty_tests, missing_output_is_dirty) {
  short_linear_graph_t g = uf->g.short_linear;
  touch_node(g.graph, g.a);
  spn_bg_dirty_t* dirty = spn_bg_compute_dirty(g.graph);

  expect_dirty(utest_result, g.graph, dirty, (expected_dirty_t) {
    .files = {
      .dirty = { g.b },
      .clean = { g.a },
    },
    .commands = {
      .dirty = { g.compile }
    }
  });
}

UTEST_F(spn_dirty_tests, input_newer_than_output_is_dirty) {
  short_linear_graph_t g = uf->g.short_linear;
  touch_node(g.graph, g.b);
  touch_node(g.graph, g.a);
  spn_bg_dirty_t* dirty = spn_bg_compute_dirty(g.graph);

  expect_dirty(utest_result, g.graph, dirty, (expected_dirty_t) {
    .files = {
      .dirty = { g.b },
      .clean = { g.a },
    },
    .commands = {
      .dirty = { g.compile }
    }
  });
}

UTEST_F(spn_dirty_tests, output_newer_than_input_is_clean) {
  short_linear_graph_t g = uf->g.short_linear;
  touch_node(g.graph, g.a);
  touch_node(g.graph, g.b);
  spn_bg_dirty_t* dirty = spn_bg_compute_dirty(g.graph);

  expect_dirty(utest_result, g.graph, dirty, (expected_dirty_t) {
    .files = {
      .clean = { g.b, g.a },
    },
    .commands = {
      .clean = { g.compile }
    }
  });
}

UTEST_F(spn_dirty_tests, long_linear_dirty_propagates) {
  long_linear_graph_t g = uf->g.long_linear;

  touch_node(g.graph, g.b);
  touch_node(g.graph, g.c);
  touch_node(g.graph, g.a);

  spn_bg_dirty_t* dirty = spn_bg_compute_dirty(g.graph);

  expect_dirty(utest_result, g.graph, dirty, (expected_dirty_t) {
    .files = {
      .dirty = { g.b, g.c },
      .clean = { g.a },
    },
    .commands = {
      .dirty = { g.compile_1, g.compile_2 },
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

  expect_dirty(utest_result, g.graph, dirty, (expected_dirty_t) {
    .files = {
      .dirty = { g.c },
      .clean = { g.a, g.b },
    },
    .commands = {
      .clean = { g.compile_1 },
      .dirty = { g.compile_2 },
    },
  });
}

UTEST_F(spn_dirty_tests, diamond_propagation) {
  diamond_graph_t g = uf->g.diamond;
  touch_node(g.graph, g.a);

  spn_bg_dirty_t* dirty = spn_bg_compute_dirty(g.graph);

  expect_dirty(utest_result, g.graph, dirty, (expected_dirty_t) {
    .files = {
      .dirty = { g.b, g.c, g.d },
      .clean = { g.a },
    },
    .commands = {
      .dirty = { g.compile_1, g.compile_2, g.join },
    },
  });
}

UTEST_F(spn_dirty_tests, fork_join_partial_missing) {
  fork_join_graph_t g = uf->g.fork_join;

  touch_node(g.graph, g.c);
  touch_node(g.graph, g.e);
  touch_node(g.graph, g.d);
  touch_node(g.graph, g.a);

  spn_bg_dirty_t* dirty = spn_bg_compute_dirty(g.graph);

  expect_dirty(utest_result, g.graph, dirty, (expected_dirty_t) {
    .files = {
      .dirty = { g.b, g.d },
      .clean = { g.a, g.c, g.e },
    },
    .commands = {
      .dirty = { g.compile_1, g.join },
      .clean = { g.compile_2 },
    },
  });
}

UTEST_F(spn_dirty_tests, fork_join_partial_rhs) {
  fork_join_graph_t g = uf->g.fork_join;

  touch_node(g.graph, g.a);
  touch_node(g.graph, g.c);
  touch_node(g.graph, g.e);
  touch_node(g.graph, g.d);
  touch_node(g.graph, g.b);

  spn_bg_dirty_t* dirty = spn_bg_compute_dirty(g.graph);

  expect_dirty(utest_result, g.graph, dirty, (expected_dirty_t) {
    .files = {
      .dirty = { g.d },
      .clean = { g.a, g.c, g.e, g.b },
    },
    .commands = {
      .dirty = { g.join },
      .clean = { g.compile_1, g.compile_2 },
    },
  });
}

UTEST_F(spn_dirty_tests, fork_join_partial_lhs) {
  fork_join_graph_t g = uf->g.fork_join;

  touch_node(g.graph, g.c);
  touch_node(g.graph, g.a);
  touch_node(g.graph, g.b);
  touch_node(g.graph, g.d);
  touch_node(g.graph, g.e);

  spn_bg_dirty_t* dirty = spn_bg_compute_dirty(g.graph);

  expect_dirty(utest_result, g.graph, dirty, (expected_dirty_t) {
    .files = {
      .dirty = { g.d },
      .clean = { g.a, g.c, g.e, g.b },
    },
    .commands = {
      .dirty = { g.join },
      .clean = { g.compile_1, g.compile_2 },
    },
  });
}

UTEST_F(spn_dirty_tests, multi_output_missing_peer) {
  multi_output_graph_t g = uf->g.multi_output;

  touch_node(g.graph, g.a);
  touch_node(g.graph, g.c);

  spn_bg_dirty_t* dirty = spn_bg_compute_dirty(g.graph);

  expect_dirty(utest_result, g.graph, dirty, (expected_dirty_t) {
    .files = {
      .dirty = { g.b, g.c },
      .clean = { g.a },
    },
    .commands = {
      .dirty = { g.split },
    },
  });
}

UTEST_F(spn_dirty_tests, split_join_asymmetric) {
  split_join_graph_t g = uf->g.split_join;

  touch_node(g.graph, g.a);
  touch_node(g.graph, g.b);
  touch_node(g.graph, g.c);
  touch_node(g.graph, g.d);
  touch_node(g.graph, g.e);
  touch_node(g.graph, g.f);

  touch_node(g.graph, g.a);

  spn_bg_dirty_t* dirty = spn_bg_compute_dirty(g.graph);

  expect_dirty(utest_result, g.graph, dirty, (expected_dirty_t) {
    .files = {
      .dirty = { g.f },
    },
    .commands = {
      .dirty = { g.join_2 },
    },
  });
}

UTEST_F(spn_dirty_tests, asymmetric_long_fork_dirty) {
  asymmetric_fork_graph_t g = uf->g.asymmetric_fork;

  touch_node(g.graph, g.a);
  touch_node(g.graph, g.b);
  touch_node(g.graph, g.c);
  touch_node(g.graph, g.d);
  touch_node(g.graph, g.e);
  touch_node(g.graph, g.f);
  touch_node(g.graph, g.g);

  touch_node(g.graph, g.a);

  spn_bg_dirty_t* dirty = spn_bg_compute_dirty(g.graph);

  expect_dirty(utest_result, g.graph, dirty, (expected_dirty_t) {
    .files = {
      .dirty = { g.b, g.c, g.d, g.e, g.g },
      .clean = { g.f }
    },
    .commands = {
      .dirty = { g.compile_1, g.compile_2, g.compile_3, g.compile_4, g.join },
    },
  });
}

UTEST_F(spn_dirty_tests, comb_all_dirty) {
  comb_graph_t g = uf->g.comb;

  touch_node(g.graph, g.a);

  spn_bg_dirty_t* dirty = spn_bg_compute_dirty(g.graph);

  expect_dirty(utest_result, g.graph, dirty, (expected_dirty_t) {
    .files = {
      .dirty = { g.b, g.c, g.d },
      .clean = { g.a },
    },
    .commands = {
      .dirty = { g.compile_1, g.compile_2, g.compile_3 },
    },
  });
}

UTEST_F(spn_dirty_tests, comb_partial_dirty) {
  comb_graph_t g = uf->g.comb;

  touch_node(g.graph, g.a);
  touch_node(g.graph, g.b);
  touch_node(g.graph, g.c);
  touch_node(g.graph, g.d);

  touch_node(g.graph, g.c);

  spn_bg_dirty_t* dirty = spn_bg_compute_dirty(g.graph);

  expect_dirty(utest_result, g.graph, dirty, (expected_dirty_t) {
    .files = {
      .dirty = { g.d },
      .clean = { g.a, g.b, g.c },
    },
    .commands = {
      .dirty = { g.compile_3 },
      .clean = { g.compile_1, g.compile_2 },
    },
  });
}

UTEST_F(spn_dirty_tests, no_input_missing_output) {
  no_input_graph_t g = uf->g.no_input;

  spn_bg_dirty_t* dirty = spn_bg_compute_dirty(g.graph);

  expect_dirty(utest_result, g.graph, dirty, (expected_dirty_t) {
    .files = {
      .dirty = { g.a, g.b },
    },
    .commands = {
      .dirty = { g.generate, g.compile },
    },
  });
}

//////////////
// EXECUTOR //
//////////////
typedef struct {
  spn_bg_id_t expected[SPN_DIRTY_TEST_MAX_NODE];
  spn_bg_executor_config_t config;
} expected_execution_t;

void
expect_execution(
  s32*                 utest_result,
  spn_build_graph_t*   graph,
  expected_execution_t exp
) {
  spn_bg_dirty_t* dirty = spn_bg_compute_dirty(graph);
  spn_bg_executor_t* ex = spn_bg_executor_new(graph, dirty, exp.config);
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

  expect_execution(utest_result, g.graph, (expected_execution_t) {
    .expected = { g.compile },
  });
}

UTEST_F(spn_executor_test, short_linear_clean) {
  short_linear_graph_t g = uf->graphs.short_linear;
  touch_node(g.graph, g.a);
  touch_node(g.graph, g.b);

  expect_execution(utest_result, g.graph, SP_ZERO_STRUCT(expected_execution_t));
}

UTEST_F(spn_executor_test, long_linear_chain_propagation) {
  long_linear_graph_t g = uf->graphs.long_linear;
  touch_node(g.graph, g.a);

  expect_execution(utest_result, g.graph, (expected_execution_t) {
    .expected = { g.compile_1, g.compile_2 },
  });
}

UTEST_F(spn_executor_test, long_linear_partial_dirty) {
  long_linear_graph_t g = uf->graphs.long_linear;

  touch_node(g.graph, g.a);
  touch_node(g.graph, g.b);
  touch_node(g.graph, g.c);

  touch_node(g.graph, g.b);

  expect_execution(utest_result, g.graph, (expected_execution_t) {
    .expected = { g.compile_2 },
  });
}

UTEST_F(spn_executor_test, diamond_all_dirty) {
  diamond_graph_t g = uf->graphs.diamond;
  touch_node(g.graph, g.a);

  expect_execution(utest_result, g.graph, (expected_execution_t) {
    .expected = { g.compile_1, g.compile_2, g.join },
  });
}

UTEST_F(spn_executor_test, fork_join_partial_dirty) {
  fork_join_graph_t g = uf->graphs.fork_join;

  touch_node(g.graph, g.c);
  touch_node(g.graph, g.e);
  touch_node(g.graph, g.d);
  touch_node(g.graph, g.a);

  expect_execution(utest_result, g.graph, (expected_execution_t) {
    .expected = { g.compile_1, g.join },
  });
}

UTEST_F(spn_executor_test, multi_output_all_dirty) {
  multi_output_graph_t g = uf->graphs.multi_output;
  touch_node(g.graph, g.a);

  expect_execution(utest_result, g.graph, (expected_execution_t) {
    .expected = { g.split, g.compile_1, g.compile_2 },
  });
}

UTEST_MAIN();
