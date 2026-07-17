#include "common.h"

typedef struct {
  const c8* identity;
  const c8* inputs [DAG_TEST_MAX_INPUTS];
  const c8* outputs [DAG_TEST_MAX_OUTPUTS];
} key_action_t;

typedef struct {
  bool equal;
} key_expect_t;

typedef struct {
  key_action_t a;
  key_action_t b;
  key_expect_t expect;
} key_test_t;

UTEST_EMPTY_FIXTURE(key)

static void build_action_key(s32* utest_result, spn_dag_t* g, key_action_t spec, spn_dag_digest_t* key) {
  spn_dag_id_t action = spn_dag_add_action(g, (spn_dag_action_config_t) {
    .identity = dag_test_digest(spec.identity)
  });

  sp_carr_for(spec.inputs, it) {
    if (!spec.inputs[it]) {
      break;
    }
    sp_str_t str = sp_cstr_as_str(spec.inputs[it]);
    spn_dag_id_t value = spn_dag_add_value(g, str.data, str.len);
    spn_dag_action_add_input(g, action, value);
  }

  sp_carr_for(spec.outputs, it) {
    if (!spec.outputs[it]) {
      break;
    }
    spn_dag_id_t file = spn_dag_add_file(g, sp_cstr_as_str(spec.outputs[it]));
    ASSERT_EQ(SPN_OK, spn_dag_action_add_output(g, action, file));
  }

  *key = spn_dag_weak_key(g, action);
}

static void run_test(s32* utest_result, key_test_t t) {
  sp_mem_t mem = sp_mem_os_new();
  spn_dag_digest_t a = sp_zero;
  spn_dag_digest_t b = sp_zero;
  build_action_key(utest_result, spn_dag_new(mem), t.a, &a);
  build_action_key(utest_result, spn_dag_new(mem), t.b, &b);
  EXPECT_EQ(t.expect.equal, spn_dag_digest_equal(a, b));
}

UTEST_F(key, identical_actions_match) {
  run_test(&ur, (key_test_t) {
    .a = { .identity = "I", .inputs = { "A", "B" }, .outputs = { "O" } },
    .b = { .identity = "I", .inputs = { "A", "B" }, .outputs = { "O" } },
    .expect = { .equal = true }
  });
}

UTEST_F(key, input_order_changes_key) {
  run_test(&ur, (key_test_t) {
    .a = { .inputs = { "A", "B" } },
    .b = { .inputs = { "B", "A" } },
  });
}

UTEST_F(key, input_content_changes_key) {
  run_test(&ur, (key_test_t) {
    .a = { .inputs = { "A" } },
    .b = { .inputs = { "B" } },
  });
}

UTEST_F(key, extra_input_changes_key) {
  run_test(&ur, (key_test_t) {
    .a = { .inputs = { "A" } },
    .b = { .inputs = { "A", "B" } },
  });
}

UTEST_F(key, identity_changes_key) {
  run_test(&ur, (key_test_t) {
    .a = { .identity = "A", .inputs = { "A" } },
    .b = { .identity = "B", .inputs = { "A" } },
  });
}

UTEST_F(key, output_path_changes_key) {
  run_test(&ur, (key_test_t) {
    .a = { .outputs = { "A" } },
    .b = { .outputs = { "B" } },
  });
}

UTEST_F(key, extra_output_changes_key) {
  run_test(&ur, (key_test_t) {
    .a = { .outputs = { "A" } },
    .b = { .outputs = { "A", "B" } },
  });
}
