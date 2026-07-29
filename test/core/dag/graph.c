#include "dag_test.h"

sp_test(graph, second_producer_rejected) {
  spn_dag_t* g = spn_dag_new(sp_test_arena(t));
  spn_dag_id_t a = spn_dag_add_action(g, (spn_dag_action_config_t) sp_zero);
  spn_dag_id_t b = spn_dag_add_action(g, (spn_dag_action_config_t) sp_zero);
  sp_must_eq(t, SPN_OK, spn_dag_action_add_output(g, a, spn_dag_add_file(g, sp_str_lit("O"))));
  sp_expect_eq(t, SPN_ERR_DAG_DUPLICATE_OUTPUT, spn_dag_action_add_output(g, b, spn_dag_add_file(g, sp_str_lit("O"))));
  return SP_OK;
}

sp_test(graph, slashed_output_name_rejected) {
  spn_dag_t* g = spn_dag_new(sp_test_arena(t));
  spn_dag_id_t a = spn_dag_add_action(g, (spn_dag_action_config_t) sp_zero);
  sp_expect_eq(t, SPN_ERR_DAG_OUTPUT_NAME, spn_dag_action_add_output(g, a, spn_dag_add_output(g, sp_str_lit("sub/O"))));
  return SP_OK;
}
