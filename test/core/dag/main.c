#define SP_IMPLEMENTATION
#include "sp.h"

#define UTEST_IMPLEMENTATION
#include "utest.h"

#include "test.h"
#include "dag/dag.h"

#define DAG_TEST_MAX_INPUTS 4
#define DAG_TEST_MAX_OPS 8

#define EXPECT_STR(actual, cstr) EXPECT_TRUE(sp_str_equal((actual), sp_str_view(cstr)))

typedef struct {
  const c8* data;
  const c8* hex;
} digest_test_t;

UTEST_EMPTY_FIXTURE(digest)

static void run_digest_test(s32* utest_result, digest_test_t t) {
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  sp_str_t data = sp_str_view(t.data);
  spn_dag_digest_t digest = spn_dag_digest(data.data, data.len);
  EXPECT_STR(spn_dag_digest_hex(scratch.mem, digest), t.hex);
  sp_mem_end_scratch(scratch);
}

UTEST_F(digest, empty) {
  run_digest_test(&ur, (digest_test_t) {
    .data = "",
    .hex = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"
  });
}

UTEST_F(digest, abc) {
  run_digest_test(&ur, (digest_test_t) {
    .data = "abc",
    .hex = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"
  });
}

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

typedef enum {
  FCT_OP_DONE,
  FCT_OP_FILE,
  FCT_OP_DIGEST,
  FCT_OP_SAVE,
  FCT_OP_RELOAD,
} file_cache_op_kind_t;

typedef struct {
  spn_err_t err;
} file_cache_expect_t;

typedef struct {
  file_cache_op_kind_t kind;
  const c8* path;
  const c8* blob;
  file_cache_expect_t expect;
} file_cache_op_t;

typedef struct {
  const c8* name;
  file_cache_op_t ops [DAG_TEST_MAX_OPS];
} file_cache_test_t;

UTEST_EMPTY_FIXTURE(file_cache)

static void run_file_cache_test(s32* utest_result, file_cache_test_t t) {
  tmpfs_t fs = sp_zero;
  tmpfs_init_named(&fs, t.name);
  sp_str_t table = tmpfs_get(&fs, sp_str_lit("file_cache.bin"));

  spn_dag_file_cache_t c = sp_zero;
  spn_dag_file_cache_init(&c, sp_mem_os_new());

  sp_carr_for(t.ops, it) {
    file_cache_op_t op = t.ops[it];
    if (op.kind == FCT_OP_DONE) {
      break;
    }

    switch (op.kind) {
      case FCT_OP_DONE: {
        break;
      }
      case FCT_OP_FILE: {
        tmpfs_create(&fs, sp_str_view(op.path), sp_str_view(op.blob));
        break;
      }
      case FCT_OP_DIGEST: {
        spn_dag_digest_t digest = sp_zero;
        EXPECT_EQ(op.expect.err, spn_dag_get_file_digest(&c, tmpfs_get(&fs, sp_str_view(op.path)), &digest));
        if (!op.expect.err) {
          sp_str_t blob = sp_str_view(op.blob);
          EXPECT_TRUE(spn_dag_digest_equal(digest, spn_dag_digest(blob.data, blob.len)));
        }
        break;
      }
      case FCT_OP_SAVE: {
        EXPECT_EQ(SPN_OK, spn_dag_file_cache_save(&c, table));
        break;
      }
      case FCT_OP_RELOAD: {
        spn_dag_file_cache_init(&c, sp_mem_os_new());
        EXPECT_EQ(op.expect.err, spn_dag_file_cache_load(&c, table));
        break;
      }
    }
  }

  tmpfs_deinit(&fs);
}

UTEST_F(file_cache, digest_matches_content) {
  run_file_cache_test(&ur, (file_cache_test_t) {
    .name = "file_cache_digest",
    .ops = {
      { .kind = FCT_OP_FILE, .path = "a.c", .blob = "spum" },
      { .kind = FCT_OP_DIGEST, .path = "a.c", .blob = "spum" },
    }
  });
}

UTEST_F(file_cache, repeated_digest_is_stable) {
  run_file_cache_test(&ur, (file_cache_test_t) {
    .name = "file_cache_repeat",
    .ops = {
      { .kind = FCT_OP_FILE, .path = "a.c", .blob = "spum" },
      { .kind = FCT_OP_DIGEST, .path = "a.c", .blob = "spum" },
      { .kind = FCT_OP_DIGEST, .path = "a.c", .blob = "spum" },
    }
  });
}

UTEST_F(file_cache, missing_file) {
  run_file_cache_test(&ur, (file_cache_test_t) {
    .name = "file_cache_missing",
    .ops = {
      { .kind = FCT_OP_DIGEST, .path = "absent.c", .expect = { .err = SPN_ERROR } },
    }
  });
}

UTEST_F(file_cache, save_load_roundtrip) {
  run_file_cache_test(&ur, (file_cache_test_t) {
    .name = "file_cache_roundtrip",
    .ops = {
      { .kind = FCT_OP_FILE, .path = "a.c", .blob = "spum" },
      { .kind = FCT_OP_DIGEST, .path = "a.c", .blob = "spum" },
      { .kind = FCT_OP_SAVE },
      { .kind = FCT_OP_RELOAD },
      { .kind = FCT_OP_DIGEST, .path = "a.c", .blob = "spum" },
    }
  });
}

UTEST_F(file_cache, persisted_detects_change) {
  run_file_cache_test(&ur, (file_cache_test_t) {
    .name = "file_cache_detects_change",
    .ops = {
      { .kind = FCT_OP_FILE, .path = "a.c", .blob = "spum" },
      { .kind = FCT_OP_DIGEST, .path = "a.c", .blob = "spum" },
      { .kind = FCT_OP_SAVE },
      { .kind = FCT_OP_FILE, .path = "a.c", .blob = "spum spum" },
      { .kind = FCT_OP_RELOAD },
      { .kind = FCT_OP_DIGEST, .path = "a.c", .blob = "spum spum" },
    }
  });
}

UTEST_F(file_cache, load_missing_table) {
  run_file_cache_test(&ur, (file_cache_test_t) {
    .name = "file_cache_load_missing",
    .ops = {
      { .kind = FCT_OP_RELOAD, .expect = { .err = SPN_ERROR } },
    }
  });
}

UTEST_MAIN();
