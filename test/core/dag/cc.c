#include "common.h"

typedef struct {
  spn_err_t err;
  const c8* prereqs [DAG_TEST_MAX_OPS];
} deps_expect_t;

typedef struct {
  const c8* content;
  deps_expect_t expect;
} deps_test_t;

typedef struct {
  spn_dag_obs_kind_t kind;
  const c8* path;
} cc_obs_t;

typedef struct {
  spn_err_t err;
  cc_obs_t obs [DAG_TEST_MAX_OPS];
} cc_expect_t;

typedef struct {
  const c8* name;
  const c8* dirs [DAG_TEST_MAX_INPUTS];
  const c8* prereqs [DAG_TEST_MAX_OPS];
  const c8* depfile;
  bool no_dep_output;
  cc_expect_t expect;
} cc_test_t;

UTEST_EMPTY_FIXTURE(cc)

static void run_deps_test(s32* utest_result, deps_test_t t) {
  sp_mem_t mem = sp_mem_os_new();
  sp_da(sp_str_t) prereqs = sp_da_new(mem, sp_str_t);
  spn_err_t err = spn_cc_deps_parse(mem, sp_str_view(t.content), &prereqs);
  EXPECT_EQ(t.expect.err, err);
  if (err) {
    return;
  }

  u32 count = 0;
  sp_carr_for(t.expect.prereqs, it) {
    if (!t.expect.prereqs[it]) {
      break;
    }
    count++;
  }
  ASSERT_EQ(count, (u32)sp_da_size(prereqs));
  sp_for(it, count) {
    EXPECT_STR(prereqs[it], t.expect.prereqs[it]);
  }
}

static void run_cc_test(s32* utest_result, cc_test_t t) {
  tmpfs_t fs = sp_zero;
  tmpfs_init_named(&fs, t.name);

  spn_dag_t* g = spn_dag_new(fs.mem);
  spn_dag_id_t action = spn_dag_add_action(g, (spn_dag_action_config_t) sp_zero);
  ASSERT_EQ(SPN_OK, spn_dag_action_add_output(g, action, spn_dag_add_file(g, tmpfs_get(&fs, sp_str_lit("O")))));
  if (!t.no_dep_output) {
    ASSERT_EQ(SPN_OK, spn_dag_action_add_output(g, action, spn_dag_add_file(g, tmpfs_get(&fs, sp_str_lit("O.d")))));
  }

  if (t.depfile) {
    tmpfs_create(&fs, sp_str_lit("O.d"), sp_str_view(t.depfile));
  } else if (t.prereqs[0]) {
    sp_str_t content = sp_str_lit("T:");
    sp_carr_for(t.prereqs, it) {
      if (!t.prereqs[it]) {
        break;
      }
      content = sp_fmt(fs.mem, "{} {}", sp_fmt_str(content), sp_fmt_str(tmpfs_get(&fs, sp_str_view(t.prereqs[it])))).value;
    }
    tmpfs_create(&fs, sp_str_lit("O.d"), content);
  }

  spn_cc_ctx_t ctx = { .g = g };
  sp_da_init(fs.mem, ctx.search_dirs);
  sp_carr_for(t.dirs, it) {
    if (!t.dirs[it]) {
      break;
    }
    sp_da_push(ctx.search_dirs, tmpfs_get(&fs, sp_str_view(t.dirs[it])));
  }

  sp_da(spn_dag_obs_t) obs = sp_da_new(fs.mem, spn_dag_obs_t);
  spn_err_t err = spn_cc_discover(spn_dag_find_action(g, action), &ctx, fs.mem, &obs);
  EXPECT_EQ(t.expect.err, err);
  if (!err) {
    u32 count = 0;
    sp_carr_for(t.expect.obs, it) {
      if (!t.expect.obs[it].path) {
        break;
      }
      count++;
    }
    ASSERT_EQ(count, (u32)sp_da_size(obs));
    sp_for(it, count) {
      EXPECT_EQ(t.expect.obs[it].kind, obs[it].kind);
      EXPECT_TRUE(sp_str_equal(obs[it].path, tmpfs_get(&fs, sp_str_view(t.expect.obs[it].path))));
    }
  }

  tmpfs_deinit(&fs);
}

UTEST_F(cc, deps_basic) {
  run_deps_test(&ur, (deps_test_t) {
    .content = "O: A B",
    .expect = { .prereqs = { "A", "B" } }
  });
}

UTEST_F(cc, deps_empty) {
  run_deps_test(&ur, (deps_test_t) {
    .content = ""
  });
}

UTEST_F(cc, deps_continuation) {
  run_deps_test(&ur, (deps_test_t) {
    .content = "O: A \\\n B",
    .expect = { .prereqs = { "A", "B" } }
  });
}

UTEST_F(cc, deps_crlf_continuation) {
  run_deps_test(&ur, (deps_test_t) {
    .content = "O: A \\\r\n B",
    .expect = { .prereqs = { "A", "B" } }
  });
}

UTEST_F(cc, deps_escaped_space) {
  run_deps_test(&ur, (deps_test_t) {
    .content = "O: A\\ B",
    .expect = { .prereqs = { "A B" } }
  });
}

UTEST_F(cc, deps_quoted) {
  run_deps_test(&ur, (deps_test_t) {
    .content = "O: \"A B\"",
    .expect = { .prereqs = { "A B" } }
  });
}

UTEST_F(cc, deps_unterminated_quote_fails) {
  run_deps_test(&ur, (deps_test_t) {
    .content = "O: \"A",
    .expect = { .err = SPN_ERROR }
  });
}

UTEST_F(cc, deps_escaped_dollar) {
  run_deps_test(&ur, (deps_test_t) {
    .content = "O: A$$B",
    .expect = { .prereqs = { "A$B" } }
  });
}

UTEST_F(cc, deps_escaped_hash) {
  run_deps_test(&ur, (deps_test_t) {
    .content = "O: A\\#B",
    .expect = { .prereqs = { "A#B" } }
  });
}

UTEST_F(cc, deps_missing_colon_fails) {
  run_deps_test(&ur, (deps_test_t) {
    .content = "O A",
    .expect = { .err = SPN_ERROR }
  });
}

UTEST_F(cc, deps_lone_cr_continuation_fails) {
  run_deps_test(&ur, (deps_test_t) {
    .content = "O: A \\\rB",
    .expect = { .err = SPN_ERROR }
  });
}

UTEST_F(cc, deps_multiple_rules) {
  run_deps_test(&ur, (deps_test_t) {
    .content = "O: A\nP: B",
    .expect = { .prereqs = { "A", "B" } }
  });
}

UTEST_F(cc, deps_drive_colon_target) {
  run_deps_test(&ur, (deps_test_t) {
    .content = "C:/O: A",
    .expect = { .prereqs = { "A" } }
  });
}

UTEST_F(cc, discover_prereq_becomes_file_obs) {
  run_cc_test(&ur, (cc_test_t) {
    .name = "cc_file_obs",
    .prereqs = { "H" },
    .expect = { .obs = { { SPN_DAG_OBS_FILE, "H" } } }
  });
}

UTEST_F(cc, discover_first_dir_no_probes) {
  run_cc_test(&ur, (cc_test_t) {
    .name = "cc_first_dir",
    .dirs = { "X", "Y" },
    .prereqs = { "X/H" },
    .expect = { .obs = { { SPN_DAG_OBS_FILE, "X/H" } } }
  });
}

UTEST_F(cc, discover_shadowed_prereq_probes_earlier_dirs) {
  run_cc_test(&ur, (cc_test_t) {
    .name = "cc_shadow",
    .dirs = { "X", "Y", "Z" },
    .prereqs = { "Z/H" },
    .expect = {
      .obs = {
        { SPN_DAG_OBS_FILE, "Z/H" },
        { SPN_DAG_OBS_ABSENT, "X/H" },
        { SPN_DAG_OBS_ABSENT, "Y/H" }
      }
    }
  });
}

UTEST_F(cc, discover_prereq_outside_dirs_no_probes) {
  run_cc_test(&ur, (cc_test_t) {
    .name = "cc_outside",
    .dirs = { "X" },
    .prereqs = { "H" },
    .expect = { .obs = { { SPN_DAG_OBS_FILE, "H" } } }
  });
}

UTEST_F(cc, discover_missing_depfile_fails) {
  run_cc_test(&ur, (cc_test_t) {
    .name = "cc_missing_depfile",
    .expect = { .err = SPN_ERROR }
  });
}

UTEST_F(cc, discover_no_dep_output_fails) {
  run_cc_test(&ur, (cc_test_t) {
    .name = "cc_no_dep_output",
    .no_dep_output = true,
    .expect = { .err = SPN_ERROR }
  });
}

UTEST_F(cc, discover_malformed_depfile_fails) {
  run_cc_test(&ur, (cc_test_t) {
    .name = "cc_malformed_depfile",
    .depfile = "T A",
    .expect = { .err = SPN_ERROR }
  });
}
