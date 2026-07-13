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
  spn_dag_digest_t identity = sp_zero;
  if (spec.identity) {
    sp_str_t str = sp_str_view(spec.identity);
    identity = spn_dag_digest(str.data, str.len);
  }

  spn_dag_id_t action = spn_dag_add_action(g, (spn_dag_action_config_t) {
    .identity = identity
  });

  sp_carr_for(spec.inputs, it) {
    if (!spec.inputs[it]) {
      break;
    }
    sp_str_t str = sp_str_view(spec.inputs[it]);
    spn_dag_id_t value = spn_dag_add_value(g, str.data, str.len);
    spn_dag_action_add_input(g, action, value);
  }

  sp_carr_for(spec.outputs, it) {
    if (!spec.outputs[it]) {
      break;
    }
    spn_dag_id_t file = spn_dag_add_file(g, sp_str_view(spec.outputs[it]));
    ASSERT_EQ(SPN_OK, spn_dag_action_add_output(g, action, file));
  }

  *key = spn_dag_action_key(g, action);
}

static void run_key_test(s32* utest_result, key_test_t t) {
  sp_mem_t mem = sp_mem_os_new();
  spn_dag_digest_t a = sp_zero;
  spn_dag_digest_t b = sp_zero;
  build_action_key(utest_result, spn_dag_new(mem), t.a, &a);
  build_action_key(utest_result, spn_dag_new(mem), t.b, &b);
  EXPECT_EQ(t.expect.equal, spn_dag_digest_equal(a, b));
}

UTEST_F(key, identical_actions_match) {
  run_key_test(&ur, (key_test_t) {
    .a = { .identity = "cc -c", .inputs = { "main.c", "sp.h" }, .outputs = { "main.o" } },
    .b = { .identity = "cc -c", .inputs = { "main.c", "sp.h" }, .outputs = { "main.o" } },
    .expect = { .equal = true }
  });
}

UTEST_F(key, input_order_changes_key) {
  run_key_test(&ur, (key_test_t) {
    .a = { .inputs = { "main.c", "sp.h" } },
    .b = { .inputs = { "sp.h", "main.c" } },
  });
}

UTEST_F(key, input_content_changes_key) {
  run_key_test(&ur, (key_test_t) {
    .a = { .inputs = { "main.c" } },
    .b = { .inputs = { "main.d" } },
  });
}

UTEST_F(key, extra_input_changes_key) {
  run_key_test(&ur, (key_test_t) {
    .a = { .inputs = { "main.c" } },
    .b = { .inputs = { "main.c", "sp.h" } },
  });
}

UTEST_F(key, identity_changes_key) {
  run_key_test(&ur, (key_test_t) {
    .a = { .identity = "cc -c -O0", .inputs = { "main.c" } },
    .b = { .identity = "cc -c -O2", .inputs = { "main.c" } },
  });
}

UTEST_F(key, output_path_changes_key) {
  run_key_test(&ur, (key_test_t) {
    .a = { .outputs = { "main.o" } },
    .b = { .outputs = { "spum.o" } },
  });
}

UTEST_F(key, extra_output_changes_key) {
  run_key_test(&ur, (key_test_t) {
    .a = { .outputs = { "main.o" } },
    .b = { .outputs = { "main.o", "main.d" } },
  });
}
