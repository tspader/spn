#include "common.h"

typedef struct {
  const c8* path;
  const c8* content;
  spn_dag_obs_kind_t kind;
} strong_key_obs_t;

typedef struct {
  const c8* prelim;
  strong_key_obs_t obs [DAG_TEST_MAX_INPUTS];
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
  spn_dag_digest_t prelim = sp_zero;
  if (spec.prelim) {
    sp_str_t str = sp_str_view(spec.prelim);
    prelim = spn_dag_digest(str.data, str.len);
  }

  spn_dag_obs_t obs [DAG_TEST_MAX_INPUTS] = sp_zero;
  u32 count = 0;
  sp_carr_for(spec.obs, it) {
    if (!spec.obs[it].path) {
      break;
    }
    obs[count] = (spn_dag_obs_t) {
      .kind = spec.obs[it].kind,
      .path = sp_str_view(spec.obs[it].path)
    };
    if (spec.obs[it].content) {
      sp_str_t content = sp_str_view(spec.obs[it].content);
      obs[count].meta.digest = spn_dag_digest(content.data, content.len);
    }
    count++;
  }

  return spn_dag_strong_key(prelim, obs, count);
}

static void run_strong_key_test(s32* utest_result, strong_key_test_t t) {
  spn_dag_digest_t a = build_strong_key(t.a);
  spn_dag_digest_t b = build_strong_key(t.b);
  EXPECT_EQ(t.expect.equal, spn_dag_digest_equal(a, b));
}

UTEST_F(strong_key, identical_folds_match) {
  run_strong_key_test(&ur, (strong_key_test_t) {
    .a = { .prelim = "cc main.c", .obs = { { "sp.h", "SP" }, { "io.h", "IO" } } },
    .b = { .prelim = "cc main.c", .obs = { { "sp.h", "SP" }, { "io.h", "IO" } } },
    .expect = { .equal = true }
  });
}

UTEST_F(strong_key, discovered_content_changes_key) {
  run_strong_key_test(&ur, (strong_key_test_t) {
    .a = { .prelim = "cc main.c", .obs = { { "sp.h", "v1" } } },
    .b = { .prelim = "cc main.c", .obs = { { "sp.h", "v2" } } },
  });
}

UTEST_F(strong_key, discovered_path_changes_key) {
  run_strong_key_test(&ur, (strong_key_test_t) {
    .a = { .prelim = "cc main.c", .obs = { { "inc1/sp.h", "SP" } } },
    .b = { .prelim = "cc main.c", .obs = { { "inc2/sp.h", "SP" } } },
  });
}

UTEST_F(strong_key, prelim_changes_key) {
  run_strong_key_test(&ur, (strong_key_test_t) {
    .a = { .prelim = "cc -O0 main.c", .obs = { { "sp.h", "SP" } } },
    .b = { .prelim = "cc -O2 main.c", .obs = { { "sp.h", "SP" } } },
  });
}

UTEST_F(strong_key, discovered_order_changes_key) {
  run_strong_key_test(&ur, (strong_key_test_t) {
    .a = { .prelim = "cc main.c", .obs = { { "sp.h", "SP" }, { "io.h", "IO" } } },
    .b = { .prelim = "cc main.c", .obs = { { "io.h", "IO" }, { "sp.h", "SP" } } },
  });
}

UTEST_F(strong_key, extra_discovered_changes_key) {
  run_strong_key_test(&ur, (strong_key_test_t) {
    .a = { .prelim = "cc main.c", .obs = { { "sp.h", "SP" } } },
    .b = { .prelim = "cc main.c", .obs = { { "sp.h", "SP" }, { "io.h", "IO" } } },
  });
}

UTEST_F(strong_key, empty_differs_from_folded) {
  run_strong_key_test(&ur, (strong_key_test_t) {
    .a = { .prelim = "cc main.c" },
    .b = { .prelim = "cc main.c", .obs = { { "sp.h", "SP" } } },
  });
}

UTEST_F(strong_key, obs_kind_changes_key) {
  run_strong_key_test(&ur, (strong_key_test_t) {
    .a = { .prelim = "cc main.c", .obs = { { "inc1/sp.h", SP_NULLPTR, SPN_DAG_OBS_ABSENT } } },
    .b = { .prelim = "cc main.c", .obs = { { "inc1/sp.h", SP_NULLPTR, SPN_DAG_OBS_FILE } } },
  });
}

UTEST_F(strong_key, probe_now_present_changes_key) {
  run_strong_key_test(&ur, (strong_key_test_t) {
    .a = { .prelim = "cc main.c", .obs = { { "inc1/sp.h", SP_NULLPTR, SPN_DAG_OBS_ABSENT }, { "inc2/sp.h", "SP" } } },
    .b = { .prelim = "cc main.c", .obs = { { "inc1/sp.h", "SP", SPN_DAG_OBS_ABSENT }, { "inc2/sp.h", "SP" } } },
  });
}
