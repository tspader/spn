#include "dag_test.h"

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
  const c8* name;
  store_expect_t expect;
} store_op_t;

typedef struct {
  const c8* name;
  store_op_t ops [DAG_TEST_MAX_OPS];
} store_test_t;

static const store_test_t store_tests [] = {
  {
    .name = "put_then_get",
    .ops = {
      { .kind = STORE_OP_PUT, .blob = "A" },
      { .kind = STORE_OP_HAS, .blob = "A", .expect = { .has = true } },
      { .kind = STORE_OP_GET, .blob = "A" },
    }
  },
  {
    .name = "empty_blob",
    .ops = {
      { .kind = STORE_OP_PUT, .blob = "" },
      { .kind = STORE_OP_GET, .blob = "" },
    }
  },
  {
    .name = "put_is_idempotent",
    .ops = {
      { .kind = STORE_OP_PUT, .blob = "A" },
      { .kind = STORE_OP_PUT, .blob = "A" },
      { .kind = STORE_OP_GET, .blob = "A" },
    }
  },
  {
    .name = "missing_digest",
    .ops = {
      { .kind = STORE_OP_HAS, .blob = "A" },
      { .kind = STORE_OP_GET, .blob = "A", .expect = { .err = SPN_ERR_DAG_STORE_MISSING } },
      { .kind = STORE_OP_MATERIALIZE, .blob = "A", .path = "a.bin", .expect = { .err = SPN_ERR_DAG_STORE_MISSING } },
    }
  },
  {
    .name = "put_file_matches_put",
    .ops = {
      { .kind = STORE_OP_FILE, .blob = "A", .path = "a.c" },
      { .kind = STORE_OP_PUT_FILE, .blob = "A", .path = "a.c" },
      { .kind = STORE_OP_GET, .blob = "A", .name = "a.c" },
    }
  },
  {
    .name = "put_file_dot_name",
    .ops = {
      { .kind = STORE_OP_FILE, .blob = "A", .path = ".gitignore" },
      { .kind = STORE_OP_PUT_FILE, .blob = "A", .path = ".gitignore" },
      { .kind = STORE_OP_HAS, .blob = "A", .name = ".gitignore", .expect = { .has = true } },
      { .kind = STORE_OP_GET, .blob = "A", .name = ".gitignore" },
    }
  },
  {
    .name = "put_file_temp_pattern_name",
    .ops = {
      { .kind = STORE_OP_FILE, .blob = "B", .path = ".b.123.4.tmp" },
      { .kind = STORE_OP_PUT_FILE, .blob = "B", .path = ".b.123.4.tmp" },
      { .kind = STORE_OP_HAS, .blob = "B", .name = ".b.123.4.tmp", .expect = { .has = true } },
      { .kind = STORE_OP_GET, .blob = "B", .name = ".b.123.4.tmp" },
    }
  },
  {
    .name = "put_file_missing",
    .ops = {
      { .kind = STORE_OP_PUT_FILE, .blob = "A", .path = "a.bin", .expect = { .err = SPN_ERR_DAG_STORE_READ } },
    }
  },
  {
    .name = "materialize",
    .ops = {
      { .kind = STORE_OP_PUT, .blob = "A" },
      { .kind = STORE_OP_MATERIALIZE, .blob = "A", .path = "a.bin" },
    }
  },
};

static sp_err_t store_run_ops(sp_test_t* t, spn_dag_store_kind_t kind, const store_test_t* test) {
  sp_test_kv_c(t, "store", dag_test_store_name(kind));

  dag_test_env_t env;
  dag_test_env_init(&env, t, (dag_test_env_config_t) {
    .sub = dag_test_store_name(kind),
    .store = kind
  });
  sp_mem_t mem = env.mem;

  sp_carr_for(test->ops, it) {
    store_op_t op = test->ops[it];
    if (op.kind == STORE_OP_DONE) {
      break;
    }

    sp_str_t blob = sp_str_view(op.blob);
    spn_dag_digest_t digest = dag_test_digest(op.blob);
    sp_str_t name = op.name ? sp_str_view(op.name) : sp_str_lit("blob");

    switch (op.kind) {
      case STORE_OP_DONE: {
        break;
      }
      case STORE_OP_FILE: {
        dag_test_env_create(&env, sp_str_view(op.path), blob);
        break;
      }
      case STORE_OP_PUT: {
        spn_dag_digest_t returned = sp_zero;
        sp_expect_eq(t, op.expect.err, spn_dag_store_put(&env.store, blob.data, blob.len, name, &returned));
        if (!op.expect.err) {
          sp_expect(t, spn_dag_digest_equal(digest, returned));
        }
        break;
      }
      case STORE_OP_PUT_FILE: {
        spn_dag_digest_t returned = sp_zero;
        sp_str_t file_name = op.name ? sp_str_view(op.name) : sp_str_view(op.path);
        sp_expect_eq(t, op.expect.err, spn_dag_store_put_file(&env.store, dag_test_env_path(&env, sp_str_view(op.path)), file_name, &returned));
        if (!op.expect.err) {
          sp_expect(t, spn_dag_digest_equal(digest, returned));
        }
        break;
      }
      case STORE_OP_GET: {
        sp_mem_slice_t fetched = sp_zero;
        sp_expect_eq(t, op.expect.err, spn_dag_store_get(&env.store, digest, name, mem, &fetched));
        if (!op.expect.err) {
          sp_expect_str_eq_c(t, sp_str((const c8*)fetched.data, (u32)fetched.len), op.blob);
        }
        break;
      }
      case STORE_OP_HAS: {
        sp_expect_eq(t, op.expect.has, spn_dag_store_has(&env.store, digest, name));
        break;
      }
      case STORE_OP_MATERIALIZE: {
        sp_str_t path = dag_test_env_path(&env, sp_str_view(op.path));
        sp_expect_eq(t, op.expect.err, spn_dag_store_materialize(&env.store, digest, name, path));
        if (!op.expect.err) {
          sp_err_t err = dag_test_expect_file(t, mem, path, op.blob);
          if (err) {
            return err;
          }
        }
        break;
      }
    }
  }

  return SP_OK;
}

sp_test_each(store, ops, store_test_t, store_tests) {
  sp_carr_for(dag_test_store_kinds, kind) {
    sp_err_t err = store_run_ops(t, dag_test_store_kinds[kind], it);
    if (err) {
      return err;
    }
  }
  return SP_OK;
}
