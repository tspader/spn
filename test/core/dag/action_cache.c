typedef struct {
  const c8* path;
  const c8* blob;
} cache_output_t;

typedef enum {
  CACHE_OP_DONE,
  CACHE_OP_PUT,
  CACHE_OP_GET,
  CACHE_OP_SAVE,
  CACHE_OP_RELOAD,
} cache_op_kind_t;

typedef struct {
  bool hit;
  spn_err_t err;
} cache_expect_t;

typedef struct {
  cache_op_kind_t kind;
  const c8* key;
  cache_output_t outputs [DAG_TEST_MAX_OUTPUTS];
  cache_expect_t expect;
} cache_op_t;

typedef struct {
  const c8* name;
  cache_op_t ops [DAG_TEST_MAX_OPS];
} cache_test_t;

UTEST_EMPTY_FIXTURE(action_cache)

static u32 cache_output_count(cache_op_t* op) {
  u32 count = 0;
  sp_carr_for(op->outputs, it) {
    if (!op->outputs[it].path) {
      break;
    }
    count++;
  }
  return count;
}

static spn_dag_digest_t cache_blob_digest(const c8* blob) {
  sp_str_t str = sp_str_view(blob);
  return spn_dag_digest(str.data, str.len);
}

static void run_cache_test(s32* utest_result, cache_test_t t) {
  tmpfs_t fs = sp_zero;
  tmpfs_init_named(&fs, t.name);
  sp_str_t table = tmpfs_get(&fs, sp_str_lit("actions.bin"));

  spn_dag_action_cache_t c = sp_zero;
  spn_dag_action_cache_init(&c, sp_mem_os_new());

  sp_carr_for(t.ops, it) {
    cache_op_t op = t.ops[it];
    if (op.kind == CACHE_OP_DONE) {
      break;
    }

    switch (op.kind) {
      case CACHE_OP_DONE: {
        break;
      }
      case CACHE_OP_PUT: {
        spn_dag_action_output_t outputs [DAG_TEST_MAX_OUTPUTS] = sp_zero;
        u32 count = cache_output_count(&op);
        sp_for(o, count) {
          outputs[o] = (spn_dag_action_output_t) {
            .path = sp_str_view(op.outputs[o].path),
            .digest = cache_blob_digest(op.outputs[o].blob)
          };
        }
        spn_dag_action_cache_put(&c, cache_blob_digest(op.key), outputs, count);
        break;
      }
      case CACHE_OP_GET: {
        spn_dag_action_entry_t* entry = spn_dag_action_cache_get(&c, cache_blob_digest(op.key));
        EXPECT_EQ(op.expect.hit, entry != SP_NULLPTR);
        if (op.expect.hit && entry) {
          u32 count = cache_output_count(&op);
          ASSERT_EQ(count, (u32)sp_da_size(entry->outputs));
          sp_for(o, count) {
            EXPECT_STR(entry->outputs[o].path, op.outputs[o].path);
            EXPECT_TRUE(spn_dag_digest_equal(entry->outputs[o].digest, cache_blob_digest(op.outputs[o].blob)));
          }
        }
        break;
      }
      case CACHE_OP_SAVE: {
        EXPECT_EQ(op.expect.err, spn_dag_action_cache_save(&c, table));
        break;
      }
      case CACHE_OP_RELOAD: {
        spn_dag_action_cache_init(&c, sp_mem_os_new());
        EXPECT_EQ(op.expect.err, spn_dag_action_cache_load(&c, table));
        break;
      }
    }
  }

  tmpfs_deinit(&fs);
}

UTEST_F(action_cache, get_missing) {
  run_cache_test(&ur, (cache_test_t) {
    .name = "action_cache_missing",
    .ops = {
      { .kind = CACHE_OP_GET, .key = "cc main.c" },
    }
  });
}

UTEST_F(action_cache, put_then_get) {
  run_cache_test(&ur, (cache_test_t) {
    .name = "action_cache_put_get",
    .ops = {
      { .kind = CACHE_OP_PUT, .key = "cc main.c", .outputs = { { "main.o", "obj" }, { "main.d", "deps" } } },
      { .kind = CACHE_OP_GET, .key = "cc main.c", .outputs = { { "main.o", "obj" }, { "main.d", "deps" } }, .expect = { .hit = true } },
    }
  });
}

UTEST_F(action_cache, put_overwrites) {
  run_cache_test(&ur, (cache_test_t) {
    .name = "action_cache_overwrite",
    .ops = {
      { .kind = CACHE_OP_PUT, .key = "cc main.c", .outputs = { { "main.o", "obj" } } },
      { .kind = CACHE_OP_PUT, .key = "cc main.c", .outputs = { { "main.o", "obj2" } } },
      { .kind = CACHE_OP_GET, .key = "cc main.c", .outputs = { { "main.o", "obj2" } }, .expect = { .hit = true } },
    }
  });
}

UTEST_F(action_cache, distinct_keys) {
  run_cache_test(&ur, (cache_test_t) {
    .name = "action_cache_distinct",
    .ops = {
      { .kind = CACHE_OP_PUT, .key = "cc main.c", .outputs = { { "main.o", "obj" } } },
      { .kind = CACHE_OP_PUT, .key = "cc spum.c", .outputs = { { "spum.o", "spum" } } },
      { .kind = CACHE_OP_GET, .key = "cc main.c", .outputs = { { "main.o", "obj" } }, .expect = { .hit = true } },
      { .kind = CACHE_OP_GET, .key = "cc spum.c", .outputs = { { "spum.o", "spum" } }, .expect = { .hit = true } },
    }
  });
}

UTEST_F(action_cache, empty_outputs) {
  run_cache_test(&ur, (cache_test_t) {
    .name = "action_cache_empty",
    .ops = {
      { .kind = CACHE_OP_PUT, .key = "cc main.c" },
      { .kind = CACHE_OP_GET, .key = "cc main.c", .expect = { .hit = true } },
    }
  });
}

UTEST_F(action_cache, save_load_roundtrip) {
  run_cache_test(&ur, (cache_test_t) {
    .name = "action_cache_roundtrip",
    .ops = {
      { .kind = CACHE_OP_PUT, .key = "cc main.c", .outputs = { { "main.o", "obj" }, { "main.d", "deps" } } },
      { .kind = CACHE_OP_PUT, .key = "cc spum.c", .outputs = { { "spum.o", "spum" } } },
      { .kind = CACHE_OP_SAVE },
      { .kind = CACHE_OP_RELOAD },
      { .kind = CACHE_OP_GET, .key = "cc main.c", .outputs = { { "main.o", "obj" }, { "main.d", "deps" } }, .expect = { .hit = true } },
      { .kind = CACHE_OP_GET, .key = "cc spum.c", .outputs = { { "spum.o", "spum" } }, .expect = { .hit = true } },
    }
  });
}

UTEST_F(action_cache, load_missing_table) {
  run_cache_test(&ur, (cache_test_t) {
    .name = "action_cache_load_missing",
    .ops = {
      { .kind = CACHE_OP_RELOAD, .expect = { .err = SPN_ERROR } },
    }
  });
}
