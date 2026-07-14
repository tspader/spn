#include "common.h"

typedef enum {
  STORE_OP_DONE,
  STORE_OP_FILE,
  STORE_OP_PUT,
  STORE_OP_PUT_FILE,
  STORE_OP_GET,
  STORE_OP_HAS,
  STORE_OP_MATERIALIZE,
} store_op_kind_t;

typedef struct {
  spn_err_t err;
  bool has;
} store_expect_t;

typedef struct {
  store_op_kind_t kind;
  const c8* blob;
  const c8* path;
  store_expect_t expect;
} store_op_t;

typedef struct {
  const c8* name;
  store_op_t ops [DAG_TEST_MAX_OPS];
} store_test_t;

UTEST_EMPTY_FIXTURE(store)

static void run_ops(s32* utest_result, spn_dag_store_kind_t kind, store_test_t t) {
  dag_test_env_t env = sp_zero;
  dag_test_env_init(&env, (dag_test_env_config_t) { .name = t.name, .store = kind });
  sp_mem_t mem = env.fs.mem;

  sp_carr_for(t.ops, it) {
    store_op_t op = t.ops[it];
    if (op.kind == STORE_OP_DONE) {
      break;
    }

    sp_str_t blob = sp_str_view(op.blob);
    spn_dag_digest_t digest = dag_test_digest(op.blob);

    switch (op.kind) {
      case STORE_OP_DONE: {
        break;
      }
      case STORE_OP_FILE: {
        tmpfs_create(&env.fs, sp_str_view(op.path), blob);
        break;
      }
      case STORE_OP_PUT: {
        spn_dag_digest_t returned = sp_zero;
        EXPECT_EQ(op.expect.err, spn_dag_put(&env.store, blob.data, blob.len, &returned));
        if (!op.expect.err) {
          EXPECT_TRUE(spn_dag_digest_equal(digest, returned));
        }
        break;
      }
      case STORE_OP_PUT_FILE: {
        spn_dag_digest_t returned = sp_zero;
        EXPECT_EQ(op.expect.err, spn_dag_store_put_file(&env.store, tmpfs_get(&env.fs, sp_str_view(op.path)), &returned));
        if (!op.expect.err) {
          EXPECT_TRUE(spn_dag_digest_equal(digest, returned));
        }
        break;
      }
      case STORE_OP_GET: {
        sp_mem_slice_t fetched = sp_zero;
        EXPECT_EQ(op.expect.err, spn_dag_store_get(&env.store, digest, mem, &fetched));
        if (!op.expect.err) {
          EXPECT_STR(sp_str((const c8*)fetched.data, (u32)fetched.len), op.blob);
        }
        break;
      }
      case STORE_OP_HAS: {
        EXPECT_EQ(op.expect.has, spn_dag_store_has(&env.store, digest));
        break;
      }
      case STORE_OP_MATERIALIZE: {
        sp_str_t path = tmpfs_get(&env.fs, sp_str_view(op.path));
        EXPECT_EQ(op.expect.err, spn_dag_store_materialize(&env.store, digest, path));
        if (!op.expect.err) {
          dag_test_expect_file(utest_result, mem, path, op.blob);
        }
        break;
      }
    }
  }

  dag_test_env_deinit(&env);
}

static void run_test(s32* utest_result, store_test_t t) {
  sp_carr_for(dag_test_store_kinds, it) {
    run_ops(utest_result, dag_test_store_kinds[it], t);
  }
}

UTEST_F(store, put_then_get) {
  run_test(&ur, (store_test_t) {
    .name = "put_then_get",
    .ops = {
      { .kind = STORE_OP_PUT, .blob = "A" },
      { .kind = STORE_OP_HAS, .blob = "A", .expect = { .has = true } },
      { .kind = STORE_OP_GET, .blob = "A" },
    }
  });
}

UTEST_F(store, empty_blob) {
  run_test(&ur, (store_test_t) {
    .name = "empty_blob",
    .ops = {
      { .kind = STORE_OP_PUT, .blob = "" },
      { .kind = STORE_OP_GET, .blob = "" },
    }
  });
}

UTEST_F(store, put_is_idempotent) {
  run_test(&ur, (store_test_t) {
    .name = "put_is_idempotent",
    .ops = {
      { .kind = STORE_OP_PUT, .blob = "A" },
      { .kind = STORE_OP_PUT, .blob = "A" },
      { .kind = STORE_OP_GET, .blob = "A" },
    }
  });
}

UTEST_F(store, missing_digest) {
  run_test(&ur, (store_test_t) {
    .name = "missing_digest",
    .ops = {
      { .kind = STORE_OP_HAS, .blob = "A" },
      { .kind = STORE_OP_GET, .blob = "A", .expect = { .err = SPN_ERR_DAG_STORE_MISSING } },
      { .kind = STORE_OP_MATERIALIZE, .blob = "A", .path = "a.bin", .expect = { .err = SPN_ERR_DAG_STORE_MISSING } },
    }
  });
}

UTEST_F(store, put_file_matches_put) {
  run_test(&ur, (store_test_t) {
    .name = "put_file_matches_put",
    .ops = {
      { .kind = STORE_OP_FILE, .blob = "A", .path = "a.c" },
      { .kind = STORE_OP_PUT_FILE, .blob = "A", .path = "a.c" },
      { .kind = STORE_OP_GET, .blob = "A" },
    }
  });
}

UTEST_F(store, put_file_missing) {
  run_test(&ur, (store_test_t) {
    .name = "put_file_missing",
    .ops = {
      { .kind = STORE_OP_PUT_FILE, .blob = "A", .path = "a.bin", .expect = { .err = SPN_ERR_DAG_STORE_READ } },
    }
  });
}

UTEST_F(store, materialize) {
  run_test(&ur, (store_test_t) {
    .name = "materialize",
    .ops = {
      { .kind = STORE_OP_PUT, .blob = "A" },
      { .kind = STORE_OP_MATERIALIZE, .blob = "A", .path = "a.bin" },
    }
  });
}
