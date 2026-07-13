typedef enum {
  DISCO_OP_DONE,
  DISCO_OP_PUT,
  DISCO_OP_GET,
  DISCO_OP_SAVE,
  DISCO_OP_RELOAD,
} disco_op_kind_t;

typedef struct {
  bool hit;
  spn_err_t err;
} disco_expect_t;

typedef struct {
  disco_op_kind_t kind;
  const c8* key;
  const c8* paths [DAG_TEST_MAX_INPUTS];
  const c8* absent [DAG_TEST_MAX_INPUTS];
  disco_expect_t expect;
} disco_op_t;

typedef struct {
  const c8* name;
  disco_op_t ops [DAG_TEST_MAX_OPS];
} disco_test_t;

UTEST_EMPTY_FIXTURE(discovery)

static spn_dag_digest_t disco_key_digest(const c8* key) {
  sp_str_t str = sp_str_view(key);
  return spn_dag_digest(str.data, str.len);
}

static u32 disco_build_obs(disco_op_t* op, spn_dag_obs_t* obs) {
  u32 count = 0;
  sp_carr_for(op->paths, it) {
    if (!op->paths[it]) {
      break;
    }
    obs[count++] = (spn_dag_obs_t) {
      .kind = SPN_DAG_OBS_FILE,
      .path = sp_str_view(op->paths[it])
    };
  }
  sp_carr_for(op->absent, it) {
    if (!op->absent[it]) {
      break;
    }
    obs[count++] = (spn_dag_obs_t) {
      .kind = SPN_DAG_OBS_ABSENT,
      .path = sp_str_view(op->absent[it])
    };
  }
  return count;
}

static void run_disco_test(s32* utest_result, disco_test_t t) {
  tmpfs_t fs = sp_zero;
  tmpfs_init_named(&fs, t.name);
  sp_str_t table = tmpfs_get(&fs, sp_str_lit("discovery.jsonl"));

  spn_dag_discovery_t d = sp_zero;
  spn_dag_discovery_init(&d, fs.mem);

  sp_carr_for(t.ops, it) {
    disco_op_t op = t.ops[it];
    if (op.kind == DISCO_OP_DONE) {
      break;
    }

    switch (op.kind) {
      case DISCO_OP_DONE: {
        break;
      }
      case DISCO_OP_PUT: {
        spn_dag_obs_t obs [2 * DAG_TEST_MAX_INPUTS] = sp_zero;
        u32 count = disco_build_obs(&op, obs);
        spn_dag_discovery_put(&d, disco_key_digest(op.key), obs, count);
        break;
      }
      case DISCO_OP_GET: {
        const spn_dag_pathset_t* set = spn_dag_discovery_get(&d, disco_key_digest(op.key));
        EXPECT_EQ(op.expect.hit, set != SP_NULLPTR);
        if (op.expect.hit && set) {
          spn_dag_obs_t obs [2 * DAG_TEST_MAX_INPUTS] = sp_zero;
          u32 count = disco_build_obs(&op, obs);
          ASSERT_EQ(count, (u32)sp_da_size(set->obs));
          sp_for(o, count) {
            EXPECT_EQ(obs[o].kind, set->obs[o].kind);
            EXPECT_TRUE(sp_str_equal(set->obs[o].path, obs[o].path));
          }
        }
        break;
      }
      case DISCO_OP_SAVE: {
        EXPECT_EQ(op.expect.err, spn_dag_discovery_save(&d, table));
        break;
      }
      case DISCO_OP_RELOAD: {
        spn_dag_discovery_init(&d, fs.mem);
        EXPECT_EQ(op.expect.err, spn_dag_discovery_load(&d, table));
        break;
      }
    }
  }

  tmpfs_deinit(&fs);
}

UTEST_F(discovery, get_missing) {
  run_disco_test(&ur, (disco_test_t) {
    .name = "discovery_missing",
    .ops = {
      { .kind = DISCO_OP_GET, .key = "cc main.c" },
    }
  });
}

UTEST_F(discovery, put_then_get) {
  run_disco_test(&ur, (disco_test_t) {
    .name = "discovery_put_get",
    .ops = {
      { .kind = DISCO_OP_PUT, .key = "cc main.c", .paths = { "sp.h", "io.h" } },
      { .kind = DISCO_OP_GET, .key = "cc main.c", .paths = { "sp.h", "io.h" }, .expect = { .hit = true } },
    }
  });
}

UTEST_F(discovery, empty_pathset_is_hit) {
  run_disco_test(&ur, (disco_test_t) {
    .name = "discovery_empty",
    .ops = {
      { .kind = DISCO_OP_PUT, .key = "cc main.c" },
      { .kind = DISCO_OP_GET, .key = "cc main.c", .expect = { .hit = true } },
    }
  });
}

UTEST_F(discovery, distinct_keys) {
  run_disco_test(&ur, (disco_test_t) {
    .name = "discovery_distinct",
    .ops = {
      { .kind = DISCO_OP_PUT, .key = "cc main.c", .paths = { "sp.h" } },
      { .kind = DISCO_OP_PUT, .key = "cc spum.c", .paths = { "spum.h" } },
      { .kind = DISCO_OP_GET, .key = "cc main.c", .paths = { "sp.h" }, .expect = { .hit = true } },
      { .kind = DISCO_OP_GET, .key = "cc spum.c", .paths = { "spum.h" }, .expect = { .hit = true } },
    }
  });
}

UTEST_F(discovery, put_overwrites) {
  run_disco_test(&ur, (disco_test_t) {
    .name = "discovery_overwrite",
    .ops = {
      { .kind = DISCO_OP_PUT, .key = "cc main.c", .paths = { "sp.h" } },
      { .kind = DISCO_OP_PUT, .key = "cc main.c", .paths = { "sp.h", "io.h" } },
      { .kind = DISCO_OP_GET, .key = "cc main.c", .paths = { "sp.h", "io.h" }, .expect = { .hit = true } },
    }
  });
}

UTEST_F(discovery, save_load_roundtrip) {
  run_disco_test(&ur, (disco_test_t) {
    .name = "discovery_roundtrip",
    .ops = {
      { .kind = DISCO_OP_PUT, .key = "cc main.c", .paths = { "sp.h", "io.h" }, .absent = { "inc1/sp.h" } },
      { .kind = DISCO_OP_PUT, .key = "cc spum.c" },
      { .kind = DISCO_OP_SAVE },
      { .kind = DISCO_OP_RELOAD },
      { .kind = DISCO_OP_GET, .key = "cc main.c", .paths = { "sp.h", "io.h" }, .absent = { "inc1/sp.h" }, .expect = { .hit = true } },
      { .kind = DISCO_OP_GET, .key = "cc spum.c", .expect = { .hit = true } },
    }
  });
}

UTEST_F(discovery, load_missing_table) {
  run_disco_test(&ur, (disco_test_t) {
    .name = "discovery_load_missing",
    .ops = {
      { .kind = DISCO_OP_RELOAD, .expect = { .err = SPN_ERROR } },
    }
  });
}
