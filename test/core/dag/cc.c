typedef struct {
  spn_err_t err;
  const c8* prereqs [DAG_TEST_MAX_INPUTS];
} cc_expect_t;

typedef struct {
  const c8* name;
  const c8* content;
  cc_expect_t expect;
} cc_test_t;

UTEST_EMPTY_FIXTURE(cc)

static void cc_expect_prereqs(s32* utest_result, sp_da(sp_str_t) out, const c8** prereqs) {
  u32 count = 0;
  sp_for(it, DAG_TEST_MAX_INPUTS) {
    if (!prereqs[it]) {
      break;
    }
    count++;
  }

  ASSERT_EQ(count, (u32)sp_da_size(out));
  sp_for(it, count) {
    EXPECT_STR(out[it], prereqs[it]);
  }
}

static void run_cc_test(s32* utest_result, cc_test_t t) {
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();

  sp_da(sp_str_t) out = sp_da_new(scratch.mem, sp_str_t);
  spn_err_t err = spn_cc_deps_parse(scratch.mem, sp_str_view(t.content), &out);
  EXPECT_EQ(t.expect.err, err);
  if (!err) {
    cc_expect_prereqs(utest_result, out, t.expect.prereqs);
  }

  sp_mem_end_scratch(scratch);
}

UTEST_F(cc, single_prereq) {
  run_cc_test(&ur, (cc_test_t) {
    .name = "cc_single",
    .content = "main.o: main.c\n",
    .expect = { .prereqs = { "main.c" } }
  });
}

UTEST_F(cc, headers) {
  run_cc_test(&ur, (cc_test_t) {
    .name = "cc_headers",
    .content = "main.o: main.c sp.h io.h\n",
    .expect = { .prereqs = { "main.c", "sp.h", "io.h" } }
  });
}

UTEST_F(cc, line_continuation) {
  run_cc_test(&ur, (cc_test_t) {
    .name = "cc_continuation",
    .content = "main.o: main.c \\\n  sp.h\n",
    .expect = { .prereqs = { "main.c", "sp.h" } }
  });
}

UTEST_F(cc, phony_targets_skipped) {
  run_cc_test(&ur, (cc_test_t) {
    .name = "cc_phony",
    .content = "main.o: main.c sp.h\n\nsp.h:\n",
    .expect = { .prereqs = { "main.c", "sp.h" } }
  });
}

UTEST_F(cc, no_prereqs) {
  run_cc_test(&ur, (cc_test_t) {
    .name = "cc_empty",
    .content = "main.o:\n",
  });
}

UTEST_F(cc, discover_reads_dep_output) {
  tmpfs_t fs = sp_zero;
  tmpfs_init_named(&fs, "cc_discover");

  sp_str_t dep = tmpfs_get(&fs, sp_str_lit("main.d"));
  sp_fs_create_file_str(dep, sp_str_lit("main.o: main.c sp.h\n"));

  spn_dag_t* g = spn_dag_new(fs.mem);
  spn_cc_ctx_t ctx = { .g = g };
  spn_dag_id_t action = spn_dag_add_action(g, (spn_dag_action_config_t) {
    .discover = spn_cc_discover,
    .user_data = &ctx
  });
  ASSERT_EQ(SPN_OK, spn_dag_action_add_output(g, action, spn_dag_add_file(g, tmpfs_get(&fs, sp_str_lit("main.o")))));
  ASSERT_EQ(SPN_OK, spn_dag_action_add_output(g, action, spn_dag_add_file(g, dep)));

  sp_da(spn_dag_obs_t) out = sp_da_new(fs.mem, spn_dag_obs_t);
  spn_dag_action_t* a = spn_dag_find_action(g, action);
  ASSERT_EQ(SPN_OK, spn_cc_discover(a, &ctx, fs.mem, &out));

  ASSERT_EQ(2u, (u32)sp_da_size(out));
  EXPECT_EQ(SPN_DAG_OBS_FILE, out[0].kind);
  EXPECT_STR(out[0].path, "main.c");
  EXPECT_EQ(SPN_DAG_OBS_FILE, out[1].kind);
  EXPECT_STR(out[1].path, "sp.h");

  tmpfs_deinit(&fs);
}

UTEST_F(cc, discover_probes_shadowing_dirs) {
  tmpfs_t fs = sp_zero;
  tmpfs_init_named(&fs, "cc_discover_probes");

  sp_str_t dep = tmpfs_get(&fs, sp_str_lit("main.d"));
  sp_fs_create_file_str(dep, sp_str_lit("main.o: main.c inc2/sp.h\n"));

  spn_dag_t* g = spn_dag_new(fs.mem);
  spn_cc_ctx_t ctx = { .g = g };
  sp_da_init(fs.mem, ctx.search_dirs);
  sp_da_push(ctx.search_dirs, sp_str_lit("inc1"));
  sp_da_push(ctx.search_dirs, sp_str_lit("inc2"));

  spn_dag_id_t action = spn_dag_add_action(g, (spn_dag_action_config_t) {
    .discover = spn_cc_discover,
    .user_data = &ctx
  });
  ASSERT_EQ(SPN_OK, spn_dag_action_add_output(g, action, spn_dag_add_file(g, tmpfs_get(&fs, sp_str_lit("main.o")))));
  ASSERT_EQ(SPN_OK, spn_dag_action_add_output(g, action, spn_dag_add_file(g, dep)));

  sp_da(spn_dag_obs_t) out = sp_da_new(fs.mem, spn_dag_obs_t);
  spn_dag_action_t* a = spn_dag_find_action(g, action);
  ASSERT_EQ(SPN_OK, spn_cc_discover(a, &ctx, fs.mem, &out));

  ASSERT_EQ(3u, (u32)sp_da_size(out));
  EXPECT_EQ(SPN_DAG_OBS_FILE, out[0].kind);
  EXPECT_STR(out[0].path, "main.c");
  EXPECT_EQ(SPN_DAG_OBS_FILE, out[1].kind);
  EXPECT_STR(out[1].path, "inc2/sp.h");
  EXPECT_EQ(SPN_DAG_OBS_ABSENT, out[2].kind);
  EXPECT_STR(out[2].path, "inc1/sp.h");

  tmpfs_deinit(&fs);
}
