#include "common.h"

typedef struct {
  const c8* prelim;
  dag_test_obs_t obs [DAG_TEST_MAX_INPUTS];
} strong_key_action_t;

typedef struct {
  bool equal;
} strong_key_expect_t;

typedef struct {
  strong_key_action_t a;
  strong_key_action_t b;
  strong_key_expect_t expect;
} strong_key_test_t;

UTEST_EMPTY_FIXTURE(strong_key)

static spn_dag_digest_t build_strong_key(strong_key_action_t spec) {
  spn_dag_obs_t obs [DAG_TEST_MAX_INPUTS] = sp_zero;
  u32 count = dag_test_obs_build(spec.obs, DAG_TEST_MAX_INPUTS, obs);
  return spn_dag_strong_key(dag_test_digest(spec.prelim), obs, count);
}

static void run_test(s32* utest_result, strong_key_test_t t) {
  spn_dag_digest_t a = build_strong_key(t.a);
  spn_dag_digest_t b = build_strong_key(t.b);
  EXPECT_EQ(t.expect.equal, spn_dag_digest_equal(a, b));
}

UTEST_F(strong_key, identical_folds_match) {
  run_test(&ur, (strong_key_test_t) {
    .a = { .prelim = "cc main.c", .obs = { { "sp.h", "SP" }, { "io.h", "IO" } } },
    .b = { .prelim = "cc main.c", .obs = { { "sp.h", "SP" }, { "io.h", "IO" } } },
    .expect = { .equal = true }
  });
}

UTEST_F(strong_key, discovered_content_changes_key) {
  run_test(&ur, (strong_key_test_t) {
    .a = { .prelim = "cc main.c", .obs = { { "sp.h", "v1" } } },
    .b = { .prelim = "cc main.c", .obs = { { "sp.h", "v2" } } },
  });
}

UTEST_F(strong_key, discovered_path_changes_key) {
  run_test(&ur, (strong_key_test_t) {
    .a = { .prelim = "cc main.c", .obs = { { "inc1/sp.h", "SP" } } },
    .b = { .prelim = "cc main.c", .obs = { { "inc2/sp.h", "SP" } } },
  });
}

UTEST_F(strong_key, prelim_changes_key) {
  run_test(&ur, (strong_key_test_t) {
    .a = { .prelim = "cc -O0 main.c", .obs = { { "sp.h", "SP" } } },
    .b = { .prelim = "cc -O2 main.c", .obs = { { "sp.h", "SP" } } },
  });
}

UTEST_F(strong_key, discovered_order_changes_key) {
  run_test(&ur, (strong_key_test_t) {
    .a = { .prelim = "cc main.c", .obs = { { "sp.h", "SP" }, { "io.h", "IO" } } },
    .b = { .prelim = "cc main.c", .obs = { { "io.h", "IO" }, { "sp.h", "SP" } } },
  });
}

UTEST_F(strong_key, extra_discovered_changes_key) {
  run_test(&ur, (strong_key_test_t) {
    .a = { .prelim = "cc main.c", .obs = { { "sp.h", "SP" } } },
    .b = { .prelim = "cc main.c", .obs = { { "sp.h", "SP" }, { "io.h", "IO" } } },
  });
}

UTEST_F(strong_key, empty_differs_from_folded) {
  run_test(&ur, (strong_key_test_t) {
    .a = { .prelim = "cc main.c" },
    .b = { .prelim = "cc main.c", .obs = { { "sp.h", "SP" } } },
  });
}

UTEST_F(strong_key, obs_kind_changes_key) {
  run_test(&ur, (strong_key_test_t) {
    .a = { .prelim = "cc main.c", .obs = { { "inc1/sp.h", SP_NULLPTR, SPN_DAG_OBS_ABSENT } } },
    .b = { .prelim = "cc main.c", .obs = { { "inc1/sp.h", SP_NULLPTR, SPN_DAG_OBS_FILE } } },
  });
}

UTEST_F(strong_key, probe_now_present_changes_key) {
  run_test(&ur, (strong_key_test_t) {
    .a = { .prelim = "cc main.c", .obs = { { "inc1/sp.h", SP_NULLPTR, SPN_DAG_OBS_ABSENT }, { "inc2/sp.h", "SP" } } },
    .b = { .prelim = "cc main.c", .obs = { { "inc1/sp.h", "SP", SPN_DAG_OBS_ABSENT }, { "inc2/sp.h", "SP" } } },
  });
}

UTEST_F(strong_key, enumeration_filter_changes_key) {
  run_test(&ur, (strong_key_test_t) {
    .a = { .prelim = "publish", .obs = { { "inc", "M", SPN_DAG_OBS_ENUMERATION, "*.h" } } },
    .b = { .prelim = "publish", .obs = { { "inc", "M", SPN_DAG_OBS_ENUMERATION, "*.c" } } },
  });
}

UTEST_F(strong_key, enumeration_membership_changes_key) {
  run_test(&ur, (strong_key_test_t) {
    .a = { .prelim = "publish", .obs = { { "inc", "M1", SPN_DAG_OBS_ENUMERATION, "*.h" } } },
    .b = { .prelim = "publish", .obs = { { "inc", "M2", SPN_DAG_OBS_ENUMERATION, "*.h" } } },
  });
}
