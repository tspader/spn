#include "dag_test.h"

sp_test(dag_graph, second_producer_rejected) {
  spn_path_roots_t storage = sp_zero;
  const spn_path_roots_t* roots = paths_test_roots_build((paths_test_roots_t) { .project = "/R" }, &storage);
  spn_dag_t* g = spn_dag_new(sp_test_arena(t), roots);
  spn_dag_id_t a = spn_dag_add_action(g, (spn_dag_action_config_t) { .execute = dag_test_exec_noop });
  spn_dag_id_t b = spn_dag_add_action(g, (spn_dag_action_config_t) { .execute = dag_test_exec_noop });
  sp_must_eq(t, SPN_OK, spn_dag_action_add_output(g, a, spn_dag_add_file(g, spn_path_make(roots, sp_str_lit("/R/O")))));
  sp_expect_eq(t, SPN_ERR_DAG_DUPLICATE_OUTPUT, spn_dag_action_add_output(g, b, spn_dag_add_file(g, spn_path_make(roots, sp_str_lit("/R/O")))));
  return SP_OK;
}

sp_test(dag_graph, slashed_output_name_rejected) {
  spn_path_roots_t storage = sp_zero;
  spn_dag_t* g = spn_dag_new(sp_test_arena(t), paths_test_roots_build((paths_test_roots_t) sp_zero, &storage));
  spn_dag_id_t a = spn_dag_add_action(g, (spn_dag_action_config_t) { .execute = dag_test_exec_noop });
  sp_expect_eq(t, SPN_ERR_DAG_OUTPUT_NAME, spn_dag_action_add_output(g, a, spn_dag_add_output(g, sp_str_lit("sub/O"))));
  return SP_OK;
}
