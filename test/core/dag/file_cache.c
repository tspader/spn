typedef enum {
  FILE_CACHE_OP_DONE,
  FILE_CACHE_OP_FILE,
  FILE_CACHE_OP_DIGEST,
  FILE_CACHE_OP_SAVE,
  FILE_CACHE_OP_RELOAD,
} file_cache_op_kind_t;

typedef struct {
  spn_err_t err;
  u32 entries;
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
  sp_str_t table = tmpfs_get(&fs, sp_str_lit("file_cache.jsonl"));

  spn_dag_file_cache_t c = sp_zero;
  spn_dag_file_cache_init(&c, fs.mem);

  sp_carr_for(t.ops, it) {
    file_cache_op_t op = t.ops[it];
    if (op.kind == FILE_CACHE_OP_DONE) {
      break;
    }

    switch (op.kind) {
      case FILE_CACHE_OP_DONE: {
        break;
      }
      case FILE_CACHE_OP_FILE: {
        tmpfs_create(&fs, sp_str_view(op.path), sp_str_view(op.blob));
        break;
      }
      case FILE_CACHE_OP_DIGEST: {
        spn_dag_digest_t digest = sp_zero;
        EXPECT_EQ(op.expect.err, spn_dag_get_file_digest(&c, tmpfs_get(&fs, sp_str_view(op.path)), &digest));
        if (!op.expect.err) {
          sp_str_t blob = sp_str_view(op.blob);
          EXPECT_TRUE(spn_dag_digest_equal(digest, spn_dag_digest(blob.data, blob.len)));
        }
        break;
      }
      case FILE_CACHE_OP_SAVE: {
        EXPECT_EQ(SPN_OK, spn_dag_file_cache_save(&c, table));
        break;
      }
      case FILE_CACHE_OP_RELOAD: {
        spn_dag_file_cache_init(&c, fs.mem);
        spn_err_t err = spn_dag_file_cache_load(&c, table);
        EXPECT_EQ(op.expect.err, err);
        if (!err) {
          EXPECT_EQ(op.expect.entries, (u32)sp_ht_size(c.entries));
        }
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
      { .kind = FILE_CACHE_OP_FILE, .path = "a.c", .blob = "spum" },
      { .kind = FILE_CACHE_OP_DIGEST, .path = "a.c", .blob = "spum" },
    }
  });
}

UTEST_F(file_cache, missing_file) {
  run_file_cache_test(&ur, (file_cache_test_t) {
    .name = "file_cache_missing",
    .ops = {
      { .kind = FILE_CACHE_OP_DIGEST, .path = "absent.c", .expect = { .err = SPN_ERROR } },
    }
  });
}

UTEST_F(file_cache, save_load_roundtrip) {
  run_file_cache_test(&ur, (file_cache_test_t) {
    .name = "file_cache_roundtrip",
    .ops = {
      { .kind = FILE_CACHE_OP_FILE, .path = "a.c", .blob = "spum" },
      { .kind = FILE_CACHE_OP_DIGEST, .path = "a.c", .blob = "spum" },
      { .kind = FILE_CACHE_OP_SAVE },
      { .kind = FILE_CACHE_OP_RELOAD, .expect = { .entries = 1 } },
      { .kind = FILE_CACHE_OP_DIGEST, .path = "a.c", .blob = "spum" },
    }
  });
}

UTEST_F(file_cache, persisted_detects_change) {
  run_file_cache_test(&ur, (file_cache_test_t) {
    .name = "file_cache_detects_change",
    .ops = {
      { .kind = FILE_CACHE_OP_FILE, .path = "a.c", .blob = "spum" },
      { .kind = FILE_CACHE_OP_DIGEST, .path = "a.c", .blob = "spum" },
      { .kind = FILE_CACHE_OP_SAVE },
      { .kind = FILE_CACHE_OP_FILE, .path = "a.c", .blob = "spum spum" },
      { .kind = FILE_CACHE_OP_RELOAD, .expect = { .entries = 1 } },
      { .kind = FILE_CACHE_OP_DIGEST, .path = "a.c", .blob = "spum spum" },
    }
  });
}

UTEST_F(file_cache, load_missing_table) {
  run_file_cache_test(&ur, (file_cache_test_t) {
    .name = "file_cache_load_missing",
    .ops = {
      { .kind = FILE_CACHE_OP_RELOAD, .expect = { .err = SPN_ERROR } },
    }
  });
}
