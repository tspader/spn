#define SP_IMPLEMENTATION
#define UTEST_DEFAULT_JOBS 0
#include "sp.h"
#include "test.h"
#include "utest.h"
#include "action.h"
#include "harness.h"

UTEST_MAIN()

UTEST_INITIALIZER(spn_build_init_tmpfs_top_level) {
  sp_mem_t mem = sp_mem_os_new();
  sp_str_t tmp = sp_fs_normalize_path(mem, sp_os_env_get(sp_str_lit("SPN_TEST_TMP")));
  if (sp_str_empty(tmp)) {
    tmp = sp_str_lit(".tmp");
  }

  sp_tm_epoch_t now = sp_tm_now_epoch();
  sp_str_t iso = sp_tm_epoch_to_iso8601(mem, now);
  c8* sanitized = (c8*)sp_alloc(mem, iso.len);
  sp_for(it, iso.len) {
    sanitized[it] = iso.data[it] == ':' ? '-' : iso.data[it];
  }

  tmpfs_set_top_level(sp_fs_join_path(mem, tmp, sp_str(sanitized, iso.len)));
}

SPN_TEST_SUITE(harness)

UTEST_F(harness, tmpfs) {
  tmpfs_init_named(&uf->fixture.fs, "tmpfs");

  tmpfs_create(&uf->fixture.fs, sp_str_lit("foo/bar.txt"), sp_str_lit("hello"));

  sp_str_t path = tmpfs_get(&uf->fixture.fs, sp_str_lit("foo/bar.txt"));
  EXPECT_TRUE(sp_fs_exists(path));
  EXPECT_TRUE(sp_str_starts_with(path, uf->fixture.fs.root));

  sp_str_t content = test_read_file(sp_mem_os_new(), path);
  EXPECT_TRUE(sp_str_equal(content, sp_str_lit("hello")));

  sp_str_t touched = tmpfs_touch(&uf->fixture.fs, sp_str_lit("nested/empty.txt"));
  EXPECT_TRUE(sp_fs_exists(touched));
  EXPECT_TRUE(sp_str_starts_with(touched, uf->fixture.fs.root));
}

#include "target.c"
#include "consume.c"
#include "deps.c"
#include "units.c"
#include "reexport.c"
#include "exports.c"
#include "script.c"
#include "cxx.c"
#include "options.c"
#include "worlds.c"
#include "run.c"
#include "layout.c"
#include "log.c"
#include "cli.c"
#include "compile_commands.c"
#include "platform.c"
#include "profile.c"
