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

static void run_store_ops(s32* utest_result, spn_dag_store_kind_t kind, store_test_t t) {
  tmpfs_t fs = sp_zero;
  tmpfs_init_named(&fs, sp_str_to_cstr(sp_mem_os_new(), sp_fmt(sp_mem_os_new(), "{}_{}", sp_fmt_cstr(t.name), sp_fmt_uint((u32)kind)).value));
  sp_mem_t mem = fs.mem;

  spn_dag_store_t store = sp_zero;
  spn_dag_store_init(&store, (spn_dag_store_config_t) {
    .kind = kind,
    .mem = mem,
    .dir = tmpfs_get(&fs, sp_str_lit("store"))
  });

  sp_carr_for(t.ops, it) {
    store_op_t op = t.ops[it];
    if (op.kind == STORE_OP_DONE) {
      break;
    }

    sp_str_t blob = sp_str_view(op.blob);
    spn_dag_digest_t digest = spn_dag_digest(blob.data, blob.len);

    switch (op.kind) {
      case STORE_OP_DONE: {
        break;
      }
      case STORE_OP_FILE: {
        tmpfs_create(&fs, sp_str_view(op.path), blob);
        break;
      }
      case STORE_OP_PUT: {
        spn_dag_digest_t returned = sp_zero;
        EXPECT_EQ(op.expect.err, spn_dag_put(&store, blob.data, blob.len, &returned));
        if (!op.expect.err) {
          EXPECT_TRUE(spn_dag_digest_equal(digest, returned));
        }
        break;
      }
      case STORE_OP_PUT_FILE: {
        spn_dag_digest_t returned = sp_zero;
        EXPECT_EQ(op.expect.err, spn_dag_store_put_file(&store, tmpfs_get(&fs, sp_str_view(op.path)), &returned));
        if (!op.expect.err) {
          EXPECT_TRUE(spn_dag_digest_equal(digest, returned));
        }
        break;
      }
      case STORE_OP_GET: {
        sp_mem_slice_t fetched = sp_zero;
        EXPECT_EQ(op.expect.err, spn_dag_store_get(&store, digest, mem, &fetched));
        if (!op.expect.err) {
          EXPECT_STR(sp_str((const c8*)fetched.data, (u32)fetched.len), op.blob);
        }
        break;
      }
      case STORE_OP_HAS: {
        EXPECT_EQ(op.expect.has, spn_dag_store_has(&store, digest));
        break;
      }
      case STORE_OP_MATERIALIZE: {
        sp_str_t path = tmpfs_get(&fs, sp_str_view(op.path));
        EXPECT_EQ(op.expect.err, spn_dag_store_materialize(&store, digest, path));
        if (!op.expect.err) {
          sp_str_t from_disk = sp_zero;
          ASSERT_EQ(SP_OK, sp_io_read_file(mem, path, &from_disk));
          EXPECT_STR(from_disk, op.blob);
        }
        break;
      }
    }
  }

  tmpfs_deinit(&fs);
}

static void run_store_test(s32* utest_result, store_test_t t) {
  spn_dag_store_kind_t kinds [] = { SPN_DAG_STORE_MEM, SPN_DAG_STORE_FILESYSTEM };
  sp_carr_for(kinds, it) {
    run_store_ops(utest_result, kinds[it], t);
  }
}

UTEST_F(store, put_then_get) {
  run_store_test(&ur, (store_test_t) {
    .name = "put_then_get",
    .ops = {
      { .kind = STORE_OP_PUT, .blob = "int main() {}" },
      { .kind = STORE_OP_HAS, .blob = "int main() {}", .expect = { .has = true } },
      { .kind = STORE_OP_GET, .blob = "int main() {}" },
    }
  });
}

UTEST_F(store, empty_blob) {
  run_store_test(&ur, (store_test_t) {
    .name = "empty_blob",
    .ops = {
      { .kind = STORE_OP_PUT, .blob = "" },
      { .kind = STORE_OP_GET, .blob = "" },
    }
  });
}

UTEST_F(store, put_is_idempotent) {
  run_store_test(&ur, (store_test_t) {
    .name = "put_is_idempotent",
    .ops = {
      { .kind = STORE_OP_PUT, .blob = "spum" },
      { .kind = STORE_OP_PUT, .blob = "spum" },
      { .kind = STORE_OP_GET, .blob = "spum" },
    }
  });
}

UTEST_F(store, missing_digest) {
  run_store_test(&ur, (store_test_t) {
    .name = "missing_digest",
    .ops = {
      { .kind = STORE_OP_HAS, .blob = "spum" },
      { .kind = STORE_OP_GET, .blob = "spum", .expect = { .err = SPN_ERROR } },
      { .kind = STORE_OP_MATERIALIZE, .blob = "spum", .path = "out.bin", .expect = { .err = SPN_ERROR } },
    }
  });
}

UTEST_F(store, put_file_matches_put) {
  run_store_test(&ur, (store_test_t) {
    .name = "put_file_matches_put",
    .ops = {
      { .kind = STORE_OP_FILE, .blob = "spum", .path = "src.c" },
      { .kind = STORE_OP_PUT_FILE, .blob = "spum", .path = "src.c" },
      { .kind = STORE_OP_GET, .blob = "spum" },
    }
  });
}

UTEST_F(store, put_file_missing) {
  run_store_test(&ur, (store_test_t) {
    .name = "put_file_missing",
    .ops = {
      { .kind = STORE_OP_PUT_FILE, .blob = "spum", .path = "absent.bin", .expect = { .err = SPN_ERROR } },
    }
  });
}

UTEST_F(store, materialize) {
  run_store_test(&ur, (store_test_t) {
    .name = "materialize",
    .ops = {
      { .kind = STORE_OP_PUT, .blob = "spum" },
      { .kind = STORE_OP_MATERIALIZE, .blob = "spum", .path = "out.bin" },
    }
  });
}
