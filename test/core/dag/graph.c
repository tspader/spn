#include "common.h"

UTEST_EMPTY_FIXTURE(graph)

UTEST_F(graph, second_producer_rejected) {
  spn_dag_t* g = spn_dag_new(sp_mem_os_new());
  spn_dag_id_t a = spn_dag_add_action(g, (spn_dag_action_config_t) sp_zero);
  spn_dag_id_t b = spn_dag_add_action(g, (spn_dag_action_config_t) sp_zero);
  ASSERT_EQ(SPN_OK, spn_dag_action_add_output(g, a, spn_dag_add_file(g, sp_str_lit("O"))));
  EXPECT_EQ(SPN_ERROR, spn_dag_action_add_output(g, b, spn_dag_add_file(g, sp_str_lit("O"))));
}

UTEST_F(graph, slashed_output_name_rejected) {
  spn_dag_t* g = spn_dag_new(sp_mem_os_new());
  spn_dag_id_t a = spn_dag_add_action(g, (spn_dag_action_config_t) sp_zero);
  EXPECT_EQ(SPN_ERROR, spn_dag_action_add_output(g, a, spn_dag_add_output(g, sp_str_lit("sub/O"))));
}
