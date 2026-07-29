#include "dag_test.h"

typedef struct {
  const c8* identity;
  const c8* inputs [DAG_TEST_MAX_INPUTS];
  const c8* outputs [DAG_TEST_MAX_OUTPUTS];
} key_action_t;

typedef struct {
  bool equal;
} key_expect_t;

typedef struct {
  const c8* name;
  key_action_t a;
  key_action_t b;
  key_expect_t expect;
} key_test_t;

static const key_test_t key_tests [] = {
  {
    .name = "identical_actions_match",
    .a = { .identity = "I", .inputs = { "A", "B" }, .outputs = { "O" } },
    .b = { .identity = "I", .inputs = { "A", "B" }, .outputs = { "O" } },
    .expect = { .equal = true }
  },
  {
    .name = "input_order_changes_key",
    .a = { .inputs = { "A", "B" } },
    .b = { .inputs = { "B", "A" } },
  },
  {
    .name = "input_content_changes_key",
    .a = { .inputs = { "A" } },
    .b = { .inputs = { "B" } },
  },
  {
    .name = "extra_input_changes_key",
    .a = { .inputs = { "A" } },
    .b = { .inputs = { "A", "B" } },
  },
  {
    .name = "identity_changes_key",
    .a = { .identity = "A", .inputs = { "A" } },
    .b = { .identity = "B", .inputs = { "A" } },
  },
  {
    .name = "output_path_changes_key",
    .a = { .outputs = { "A" } },
    .b = { .outputs = { "B" } },
  },
  {
    .name = "extra_output_changes_key",
    .a = { .outputs = { "A" } },
    .b = { .outputs = { "A", "B" } },
  },
};

static sp_err_t build_action_key(sp_test_t* t, spn_dag_t* g, const key_action_t* spec, spn_dag_digest_t* key) {
  spn_dag_id_t action = spn_dag_add_action(g, (spn_dag_action_config_t) {
    .identity = dag_test_digest(spec->identity)
  });

  sp_carr_for(spec->inputs, it) {
    if (!spec->inputs[it]) {
      break;
    }
    sp_str_t str = sp_cstr_as_str(spec->inputs[it]);
    spn_dag_id_t value = spn_dag_add_value(g, str.data, str.len);
    spn_dag_action_add_input(g, action, value);
  }

  sp_carr_for(spec->outputs, it) {
    if (!spec->outputs[it]) {
      break;
    }
    spn_dag_id_t file = spn_dag_add_file(g, sp_cstr_as_str(spec->outputs[it]));
    sp_must_eq(t, SPN_OK, spn_dag_action_add_output(g, action, file));
  }

  *key = spn_dag_weak_key(g, action);
  return SP_OK;
}

sp_test_each(key, weak, key_test_t, key_tests) {
  sp_mem_t mem = sp_test_arena(t);
  spn_dag_digest_t a = sp_zero;
  spn_dag_digest_t b = sp_zero;
  sp_err_t err = build_action_key(t, spn_dag_new(mem), &it->a, &a);
  if (err) {
    return err;
  }
  err = build_action_key(t, spn_dag_new(mem), &it->b, &b);
  if (err) {
    return err;
  }
  sp_expect_eq(t, it->expect.equal, spn_dag_digest_equal(a, b));
  return SP_OK;
}
