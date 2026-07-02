#define SP_IMPLEMENTATION
#include "sp.h"

#include "test.h"

#include "log/lazy/lazy.h"

///////////
// TESTS //
///////////
UTEST_MAIN()

#define LAZY_TEST_MAX_WRITES 8

typedef struct {
  const c8* content;
} lazy_log_expect_t;

typedef struct {
  const c8* seed;
  const c8* writes [LAZY_TEST_MAX_WRITES];
  lazy_log_expect_t expect;
} lazy_log_test_t;

struct lazy_log {
  tmpfs_t fs;
};

UTEST_F_SETUP(lazy_log) {}
UTEST_F_TEARDOWN(lazy_log) {}

void run_lazy_log_test(s32* utest_result, tmpfs_t* fs, lazy_log_test_t t) {
  sp_str_t path = tmpfs_get(fs, sp_str_lit("x.log"));
  if (t.seed) {
    sp_fs_create_file_str(path, sp_str_view(t.seed));
  }

  spn_lazy_log_t log;
  spn_lazy_log_init(&log, path);

  sp_carr_for(t.writes, it) {
    if (!t.writes[it]) break;
    sp_io_write_str(&log.writer, sp_str_view(t.writes[it]), SP_NULLPTR);
  }

  spn_lazy_log_close(&log);

  if (t.expect.content) {
    EXPECT_TRUE(sp_fs_exists(path));
    EXPECT_TRUE(test_read_eq(fs->mem, path, t.expect.content));
  } else {
    EXPECT_FALSE(sp_fs_exists(path));
  }
}

UTEST_F(lazy_log, never_written_creates_nothing) {
  tmpfs_init_named(&uf->fs, "lazy_never_written");
  run_lazy_log_test(utest_result, &uf->fs, (lazy_log_test_t) { 0 });
}

UTEST_F(lazy_log, never_written_preserves_existing) {
  tmpfs_init_named(&uf->fs, "lazy_preserves_existing");
  run_lazy_log_test(utest_result, &uf->fs, (lazy_log_test_t) {
    .seed = "cached diagnostics\n",
    .expect = { .content = "cached diagnostics\n" },
  });
}

UTEST_F(lazy_log, first_write_creates) {
  tmpfs_init_named(&uf->fs, "lazy_first_write");
  run_lazy_log_test(utest_result, &uf->fs, (lazy_log_test_t) {
    .writes = { "hello" },
    .expect = { .content = "hello" },
  });
}

UTEST_F(lazy_log, writes_accumulate) {
  tmpfs_init_named(&uf->fs, "lazy_accumulate");
  run_lazy_log_test(utest_result, &uf->fs, (lazy_log_test_t) {
    .writes = { "foo", "bar", "baz" },
    .expect = { .content = "foobarbaz" },
  });
}

UTEST_F(lazy_log, first_write_truncates_existing) {
  tmpfs_init_named(&uf->fs, "lazy_truncates");
  run_lazy_log_test(utest_result, &uf->fs, (lazy_log_test_t) {
    .seed = "stale stale stale stale",
    .writes = { "new" },
    .expect = { .content = "new" },
  });
}
