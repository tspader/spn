#include "common.h"

typedef enum {
  FILE_CACHE_OP_DONE,
  FILE_CACHE_OP_FILE,
  FILE_CACHE_OP_WRITE,
  FILE_CACHE_OP_REFRESH,
  FILE_CACHE_OP_INVALIDATE,
  FILE_CACHE_OP_INVALIDATE_DIR,
  FILE_CACHE_OP_DIGEST,
  FILE_CACHE_OP_SEED,
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

static void run_test(s32* utest_result, file_cache_test_t t) {
  tmpfs_t fs = sp_zero;
  tmpfs_init_named(&fs, t.name);

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
        tmpfs_create(&fs, sp_cstr_as_str(op.path), sp_cstr_as_str(op.blob));
        spn_dag_file_cache_refresh(&c);
        break;
      }
      case FILE_CACHE_OP_WRITE: {
        tmpfs_create(&fs, sp_cstr_as_str(op.path), sp_cstr_as_str(op.blob));
        break;
      }
      case FILE_CACHE_OP_REFRESH: {
        spn_dag_file_cache_refresh(&c);
        break;
      }
      case FILE_CACHE_OP_INVALIDATE: {
        spn_dag_file_cache_invalidate(&c, tmpfs_get(&fs, sp_cstr_as_str(op.path)));
        break;
      }
      case FILE_CACHE_OP_INVALIDATE_DIR: {
        spn_dag_file_cache_invalidate_dir(&c, tmpfs_get(&fs, sp_cstr_as_str(op.path)));
        break;
      }
      case FILE_CACHE_OP_DIGEST: {
        spn_dag_digest_t digest = sp_zero;
        EXPECT_EQ(op.expect.err, spn_dag_get_file_digest(&c, tmpfs_get(&fs, sp_cstr_as_str(op.path)), &digest));
        if (!op.expect.err) {
          EXPECT_TRUE(spn_dag_digest_equal(digest, dag_test_digest(op.blob)));
        }
        break;
      }
      case FILE_CACHE_OP_SEED: {
        sp_sys_file_meta_t sys = sp_zero;
        ASSERT_EQ(SPN_OK, spn_dag_get_file_meta(&c, tmpfs_get(&fs, sp_cstr_as_str(op.path)), &sys));
        spn_dag_file_cache_seed(&c, (spn_dag_file_meta_t) {
          .id = { .device = sys.device, .id = sys.id },
          .mtime = sys.mtime,
          .size = sys.size,
          .digest = dag_test_digest(op.blob)
        });
        break;
      }
    }
  }

  tmpfs_deinit(&fs);
}

UTEST_F(file_cache, digest_matches_content) {
  run_test(&ur, (file_cache_test_t) {
    .name = "file_cache_digest",
    .ops = {
      { .kind = FILE_CACHE_OP_FILE, .path = "a.c", .blob = "A" },
      { .kind = FILE_CACHE_OP_DIGEST, .path = "a.c", .blob = "A" },
    }
  });
}

UTEST_F(file_cache, missing_file) {
  run_test(&ur, (file_cache_test_t) {
    .name = "file_cache_missing",
    .ops = {
      { .kind = FILE_CACHE_OP_DIGEST, .path = "a.c", .expect = { .err = SPN_ERR_DAG_STAT } },
    }
  });
}

UTEST_F(file_cache, metadata_pinned_until_refresh) {
  run_test(&ur, (file_cache_test_t) {
    .name = "file_cache_refresh",
    .ops = {
      { .kind = FILE_CACHE_OP_FILE, .path = "F", .blob = "A" },
      { .kind = FILE_CACHE_OP_DIGEST, .path = "F", .blob = "A" },
      { .kind = FILE_CACHE_OP_WRITE, .path = "F", .blob = "BB" },
      { .kind = FILE_CACHE_OP_DIGEST, .path = "F", .blob = "A" },
      { .kind = FILE_CACHE_OP_REFRESH },
      { .kind = FILE_CACHE_OP_DIGEST, .path = "F", .blob = "BB" },
    }
  });
}

UTEST_F(file_cache, invalidate_unpins_path) {
  run_test(&ur, (file_cache_test_t) {
    .name = "file_cache_invalidate",
    .ops = {
      { .kind = FILE_CACHE_OP_FILE, .path = "F", .blob = "A" },
      { .kind = FILE_CACHE_OP_DIGEST, .path = "F", .blob = "A" },
      { .kind = FILE_CACHE_OP_WRITE, .path = "F", .blob = "BB" },
      { .kind = FILE_CACHE_OP_INVALIDATE, .path = "F" },
      { .kind = FILE_CACHE_OP_DIGEST, .path = "F", .blob = "BB" },
    }
  });
}

UTEST_F(file_cache, invalidate_dir_unpins_subtree) {
  run_test(&ur, (file_cache_test_t) {
    .name = "file_cache_invalidate_dir",
    .ops = {
      { .kind = FILE_CACHE_OP_FILE, .path = "D/F", .blob = "A" },
      { .kind = FILE_CACHE_OP_DIGEST, .path = "D/F", .blob = "A" },
      { .kind = FILE_CACHE_OP_WRITE, .path = "D/F", .blob = "BB" },
      { .kind = FILE_CACHE_OP_INVALIDATE_DIR, .path = "D" },
      { .kind = FILE_CACHE_OP_DIGEST, .path = "D/F", .blob = "BB" },
    }
  });
}

UTEST_F(file_cache, invalidate_dir_spares_siblings) {
  run_test(&ur, (file_cache_test_t) {
    .name = "file_cache_invalidate_dir_siblings",
    .ops = {
      { .kind = FILE_CACHE_OP_FILE, .path = "E/G", .blob = "C" },
      { .kind = FILE_CACHE_OP_DIGEST, .path = "E/G", .blob = "C" },
      { .kind = FILE_CACHE_OP_WRITE, .path = "E/G", .blob = "DD" },
      { .kind = FILE_CACHE_OP_INVALIDATE_DIR, .path = "D" },
      { .kind = FILE_CACHE_OP_DIGEST, .path = "E/G", .blob = "C" },
    }
  });
}

UTEST_F(file_cache, seeded_digest_trusted_without_hash) {
  run_test(&ur, (file_cache_test_t) {
    .name = "file_cache_seed",
    .ops = {
      { .kind = FILE_CACHE_OP_FILE, .path = "a.c", .blob = "A" },
      { .kind = FILE_CACHE_OP_SEED, .path = "a.c", .blob = "B" },
      { .kind = FILE_CACHE_OP_DIGEST, .path = "a.c", .blob = "B" },
    }
  });
}

UTEST_F(file_cache, seed_invalidated_by_change) {
  run_test(&ur, (file_cache_test_t) {
    .name = "file_cache_seed_stale",
    .ops = {
      { .kind = FILE_CACHE_OP_FILE, .path = "a.c", .blob = "A" },
      { .kind = FILE_CACHE_OP_SEED, .path = "a.c", .blob = "B" },
      { .kind = FILE_CACHE_OP_FILE, .path = "a.c", .blob = "C" },
      { .kind = FILE_CACHE_OP_DIGEST, .path = "a.c", .blob = "C" },
    }
  });
}
