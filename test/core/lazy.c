#include "spn_test.h"

#include "log/lazy/lazy.h"


#define LAZY_TEST_MAX_WRITES 8

typedef struct {
  const c8* content;
} expect_t;

typedef struct {
  const c8* name;
  const c8* seed;
  const c8* writes [LAZY_TEST_MAX_WRITES];
  expect_t expect;
} test_t;

static const test_t tests [] = {
  {
    .name = "never_written_creates_nothing",
  },
  {
    .name = "never_written_preserves_existing",
    .seed = "cached diagnostics\n",
    .expect = { .content = "cached diagnostics\n" },
  },
  {
    .name = "first_write_creates",
    .writes = { "hello" },
    .expect = { .content = "hello" },
  },
  {
    .name = "writes_accumulate",
    .writes = { "foo", "bar", "baz" },
    .expect = { .content = "foobarbaz" },
  },
  {
    .name = "first_write_truncates_existing",
    .seed = "stale stale stale stale",
    .writes = { "new" },
    .expect = { .content = "new" },
  },
};

sp_test_each(lazy_log, write, test_t, tests) {
  sp_mem_t mem = sp_test_arena(t);
  sp_str_t path = sp_fs_join_path(mem, sp_test_dir(t), sp_str_lit("x.log"));
  if (it->seed) {
    sp_fs_create_file_str(path, sp_str_view(it->seed));
  }

  spn_lazy_log_t log;
  spn_lazy_log_init(&log, path);

  sp_carr_for(it->writes, w) {
    if (!it->writes[w]) break;
    sp_io_write_str(&log.writer, sp_str_view(it->writes[w]), SP_NULLPTR);
  }

  spn_lazy_log_close(&log);

  if (it->expect.content) {
    sp_expect(t, sp_fs_exists(path));
    sp_expect_str_eq_c(t, test_read_file(mem, path), it->expect.content);
  } else {
    sp_expect(t, !sp_fs_exists(path));
  }

  return SP_OK;
}
