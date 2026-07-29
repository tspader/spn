#include "dag_test.h"

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

static const file_cache_test_t file_cache_tests [] = {
  {
    .name = "digest_matches_content",
    .ops = {
      { .kind = FILE_CACHE_OP_FILE, .path = "a.c", .blob = "A" },
      { .kind = FILE_CACHE_OP_DIGEST, .path = "a.c", .blob = "A" },
    }
  },
  {
    .name = "missing_file",
    .ops = {
      { .kind = FILE_CACHE_OP_DIGEST, .path = "a.c", .expect = { .err = SPN_ERR_DAG_STAT } },
    }
  },
  {
    .name = "metadata_pinned_until_refresh",
    .ops = {
      { .kind = FILE_CACHE_OP_FILE, .path = "F", .blob = "A" },
      { .kind = FILE_CACHE_OP_DIGEST, .path = "F", .blob = "A" },
      { .kind = FILE_CACHE_OP_WRITE, .path = "F", .blob = "BB" },
      { .kind = FILE_CACHE_OP_DIGEST, .path = "F", .blob = "A" },
      { .kind = FILE_CACHE_OP_REFRESH },
      { .kind = FILE_CACHE_OP_DIGEST, .path = "F", .blob = "BB" },
    }
  },
  {
    .name = "invalidate_unpins_path",
    .ops = {
      { .kind = FILE_CACHE_OP_FILE, .path = "F", .blob = "A" },
      { .kind = FILE_CACHE_OP_DIGEST, .path = "F", .blob = "A" },
      { .kind = FILE_CACHE_OP_WRITE, .path = "F", .blob = "BB" },
      { .kind = FILE_CACHE_OP_INVALIDATE, .path = "F" },
      { .kind = FILE_CACHE_OP_DIGEST, .path = "F", .blob = "BB" },
    }
  },
  {
    .name = "invalidate_dir_unpins_subtree",
    .ops = {
      { .kind = FILE_CACHE_OP_FILE, .path = "D/F", .blob = "A" },
      { .kind = FILE_CACHE_OP_DIGEST, .path = "D/F", .blob = "A" },
      { .kind = FILE_CACHE_OP_WRITE, .path = "D/F", .blob = "BB" },
      { .kind = FILE_CACHE_OP_INVALIDATE_DIR, .path = "D" },
      { .kind = FILE_CACHE_OP_DIGEST, .path = "D/F", .blob = "BB" },
    }
  },
  {
    .name = "invalidate_dir_spares_siblings",
    .ops = {
      { .kind = FILE_CACHE_OP_FILE, .path = "E/G", .blob = "C" },
      { .kind = FILE_CACHE_OP_DIGEST, .path = "E/G", .blob = "C" },
      { .kind = FILE_CACHE_OP_WRITE, .path = "E/G", .blob = "DD" },
      { .kind = FILE_CACHE_OP_INVALIDATE_DIR, .path = "D" },
      { .kind = FILE_CACHE_OP_DIGEST, .path = "E/G", .blob = "C" },
    }
  },
  {
    .name = "seeded_digest_trusted_without_hash",
    .ops = {
      { .kind = FILE_CACHE_OP_FILE, .path = "a.c", .blob = "A" },
      { .kind = FILE_CACHE_OP_SEED, .path = "a.c", .blob = "B" },
      { .kind = FILE_CACHE_OP_DIGEST, .path = "a.c", .blob = "B" },
    }
  },
  {
    .name = "seed_invalidated_by_change",
    .ops = {
      { .kind = FILE_CACHE_OP_FILE, .path = "a.c", .blob = "A" },
      { .kind = FILE_CACHE_OP_SEED, .path = "a.c", .blob = "B" },
      { .kind = FILE_CACHE_OP_FILE, .path = "a.c", .blob = "C" },
      { .kind = FILE_CACHE_OP_DIGEST, .path = "a.c", .blob = "C" },
    }
  },
};

sp_test_each(dag_file_cache, ops, file_cache_test_t, file_cache_tests) {
  sp_mem_t mem = sp_test_arena(t);
  sp_str_t root = sp_test_dir(t);

  spn_dag_file_cache_t c = sp_zero;
  spn_dag_file_cache_init(&c, mem);

  sp_carr_for(it->ops, ot) {
    file_cache_op_t op = it->ops[ot];
    if (op.kind == FILE_CACHE_OP_DONE) {
      break;
    }

    switch (op.kind) {
      case FILE_CACHE_OP_DONE: {
        break;
      }
      case FILE_CACHE_OP_FILE: {
        dag_test_create(sp_fs_join_path(mem, root, sp_cstr_as_str(op.path)), sp_cstr_as_str(op.blob));
        spn_dag_file_cache_invalidate_all(&c);
        break;
      }
      case FILE_CACHE_OP_WRITE: {
        dag_test_create(sp_fs_join_path(mem, root, sp_cstr_as_str(op.path)), sp_cstr_as_str(op.blob));
        break;
      }
      case FILE_CACHE_OP_REFRESH: {
        spn_dag_file_cache_invalidate_all(&c);
        break;
      }
      case FILE_CACHE_OP_INVALIDATE: {
        spn_dag_file_cache_invalidate(&c, sp_fs_join_path(mem, root, sp_cstr_as_str(op.path)));
        break;
      }
      case FILE_CACHE_OP_INVALIDATE_DIR: {
        spn_dag_file_cache_invalidate_dir(&c, sp_fs_join_path(mem, root, sp_cstr_as_str(op.path)));
        break;
      }
      case FILE_CACHE_OP_DIGEST: {
        spn_dag_digest_t digest = sp_zero;
        sp_expect_eq(t, op.expect.err, spn_dag_file_cache_digest(&c, sp_fs_join_path(mem, root, sp_cstr_as_str(op.path)), &digest));
        if (!op.expect.err) {
          sp_expect(t, spn_dag_digest_equal(digest, dag_test_digest(op.blob)));
        }
        break;
      }
      case FILE_CACHE_OP_SEED: {
        sp_sys_file_meta_t sys = sp_zero;
        sp_must_eq(t, SPN_OK, spn_dag_file_cache_stat(&c, sp_fs_join_path(mem, root, sp_cstr_as_str(op.path)), &sys));
        spn_dag_file_cache_seed(&c, (spn_dag_file_meta_t) {
          .id = { .device = sys.device, .inode = sys.id },
          .mtime = sys.mtime,
          .size = sys.size,
          .digest = dag_test_digest(op.blob)
        });
        break;
      }
    }
  }

  return SP_OK;
}
