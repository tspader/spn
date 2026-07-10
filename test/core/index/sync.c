#include "error/types.h"
#include "sp.h"
#include "utest.h"
#include "test.h"

#include "index/index.h"

UTEST_EMPTY_FIXTURE(index_sync)

typedef struct {
  const c8* name;

  spn_index_protocol_t protocol;
  bool cloned;
  bool mutate_remote;

  struct {
    spn_err_t result;
    bool cached;
  } expect;
} sync_case_t;

static void commit_seed(sp_str_t repo) {
  static u32 counter = 0;

  sp_mem_t mem = sp_mem_arena_as_allocator(ctx_get()->arena);
  sp_str_t file = sp_fs_join_path(mem, repo, sp_str_lit("seed.txt"));
  sp_fs_create_file_str(file, sp_fmt(mem, "{}", sp_fmt_uint(counter++)).value);

  git_repo_stage_all(repo);
  git_repo_commit(repo, sp_str_lit("seed"));
}

static void run_sync_case(s32* utest_result, sync_case_t c) {
  ctx_t* harness = ctx_get();
  sp_mem_t mem = sp_mem_arena_as_allocator(harness->arena);
  sp_str_t tmp = tmpfs_get(&harness->fs, sp_str_view(c.name));

  sp_str_t remote = sp_fs_join_path(mem, tmp, sp_str_lit("remote/index"));
  sp_str_t cache = sp_fs_join_path(mem, tmp, sp_str_lit("cache/index"));

  sp_fs_create_dir(remote);
  git_repo_init(remote);
  commit_seed(remote);

  spn_index_info_t index = {
    .name = sp_str_lit("test"),
    .url = remote,
    .location = cache,
    .protocol = c.protocol,
  };

  if (c.cloned) {
    EXPECT_EQ(SPN_OK, spn_index_sync(&index, false));
  }
  if (c.mutate_remote) {
    commit_seed(remote);
  }

  EXPECT_EQ(c.expect.result, spn_index_sync(&index, false));
  EXPECT_EQ(c.expect.cached, sp_fs_exists(cache));
}

UTEST_F(index_sync, sync_clone_when_missing) {
  run_sync_case(utest_result, (sync_case_t) {
    .name = "sync_clone_when_missing",
    .expect = {
      .cached = true,
    },
  });
}

UTEST_F(index_sync, sync_fetch_when_present) {
  run_sync_case(utest_result, (sync_case_t) {
    .name = "sync_fetch_when_present",
    .cloned = true,
    .mutate_remote = true,
    .expect = {
      .cached = true,
    },
  });
}

UTEST_F(index_sync, sync_http_unimplemented) {
  run_sync_case(utest_result, (sync_case_t) {
    .name = "sync_http_unimplemented",
    .protocol = SPN_INDEX_PROTOCOL_HTTP,
    .expect = {
      .result = SPN_ERROR,
    },
  });
}
