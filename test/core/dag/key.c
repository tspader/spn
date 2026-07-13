typedef struct {
  const c8* salt;
  const c8* inputs [DAG_TEST_MAX_INPUTS];
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

static spn_dag_digest_t build_action_key(spn_dag_t* g, key_action_t spec) {
  spn_dag_digest_t salt = sp_zero;
  if (spec.salt) {
    sp_str_t str = sp_str_view(spec.salt);
    salt = spn_dag_digest(str.data, str.len);
  }

  spn_dag_id_t action = spn_dag_add_action(g, (spn_dag_action_config_t) {
    .salt = salt
  });

  sp_carr_for(spec.inputs, it) {
    if (!spec.inputs[it]) {
      break;
    }
    sp_str_t str = sp_str_view(spec.inputs[it]);
    spn_dag_id_t value = spn_dag_add_value(g, str.data, str.len);
    spn_dag_action_add_input(g, action, value);
  }

  return spn_dag_action_key(g, action);
}

static void run_key_test(s32* utest_result, key_test_t t) {
  spn_dag_t* g = spn_dag_new(sp_mem_os_new());
  spn_dag_digest_t a = build_action_key(g, t.a);
  spn_dag_digest_t b = build_action_key(g, t.b);
  EXPECT_EQ(t.expect.equal, spn_dag_digest_equal(a, b));
}

UTEST_F(key, identical_actions_match) {
  run_key_test(&ur, (key_test_t) {
    .a = { .salt = "cc -c", .inputs = { "main.c", "sp.h" } },
    .b = { .salt = "cc -c", .inputs = { "main.c", "sp.h" } },
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

UTEST_F(key, salt_changes_key) {
  run_key_test(&ur, (key_test_t) {
    .a = { .salt = "cc -c -O0", .inputs = { "main.c" } },
    .b = { .salt = "cc -c -O2", .inputs = { "main.c" } },
  });
}
