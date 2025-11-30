#define SP_IMPLEMENTATION
#include "sp.h"
#include "graph.h"

#define SP_TEST_IMPLEMENTATION
#include "test.h"
#include "utest.h"

#define uf utest_fixture

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

struct spn_bug_tests {
  sp_test_file_manager_t fm;
};

UTEST_F_SETUP(spn_bug_tests) {
  sp_test_file_manager_init(&uf->fm);
}

UTEST_F_TEARDOWN(spn_bug_tests) {
  sp_test_file_manager_cleanup(&uf->fm);
}

UTEST_F(spn_bug_tests, id_hash_table_consistency) {
  spn_build_graph_t* g = spn_bg_new();
  spn_bg_id_t file_id = spn_bg_add_file(g, sp_str_lit("a"));

  sp_ht(spn_bg_id_t, bool) table = SP_ZERO_INITIALIZE();
  sp_ht_set_fns(table, sp_ht_on_hash_key, sp_ht_on_compare_key);

  sp_ht_insert(table, file_id, true);
  EXPECT_TRUE(sp_ht_key_exists(table, file_id));

  spn_build_file_t* file = spn_bg_find_file(g, file_id);
  EXPECT_TRUE(sp_ht_key_exists(table, file->id));
}

UTEST_F(spn_bug_tests, out_to_in_traverses_producer) {
  spn_build_graph_t* g = spn_bg_new();
  spn_bg_id_t a = spn_bg_add_file(g, sp_str_lit("a"));
  spn_bg_id_t b = spn_bg_add_file(g, sp_str_lit("b"));
  spn_bg_id_t cmd = spn_bg_add_command(g, SPN_BUILD_CMD_FN);

  spn_build_command_add_input(g, cmd, a);
  spn_build_command_add_output(g, cmd, b);

  visit_counter_t counter = SP_ZERO_INITIALIZE();
  spn_bg_dfs((spn_bg_it_config_t){
    .graph = g,
    .direction = SPN_BG_ITER_DIR_OUT_TO_IN,
    .on_cmd = count_cmd_fn,
    .on_file = count_file_fn,
    .user_data = &counter,
  });

  EXPECT_EQ(2, counter.file_count);
  EXPECT_EQ(1, counter.cmd_count);
}

UTEST_MAIN();
