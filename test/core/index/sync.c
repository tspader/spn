#include "error/types.h"
#include "sp.h"
#include "utest.h"
#include "test.h"

#include "index/index.h"

#define uf utest_fixture

typedef enum {
  INDEX_SYNC_SETUP_REMOTE_ONLY,
  INDEX_SYNC_SETUP_CLONED_REPO,
} index_sync_setup_t;

struct index_sync_fixture {
  s32 unused;
};

typedef struct {
  const c8* name;

  spn_index_protocol_t protocol;
  index_sync_setup_t setup;
  bool mutate_remote;

  struct {
    spn_err_t result;
    bool cached;
  } expect;
} case_t;

void modify_index(sp_str_t dir) {
  static u32 it = 0;

  sp_str_t file = sp_fs_join_path(dir, sp_str_lit("seed.txt"));
  sp_str_t content = sp_format("{}", SP_FMT_U32(it++));

  sp_io_writer_t io = sp_io_writer_from_file(file, SP_IO_WRITE_MODE_OVERWRITE);
  sp_io_write_str(&io, content);
  sp_io_writer_close(&io);

  git_repo_stage_all(dir);
  git_repo_commit(dir, sp_str_lit("seed"));
}

static void index_sync_prepare(s32* utest_result, case_t c, sp_str_t remote, sp_str_t cache) {

}

static void run_index_sync_case(s32* utest_result, struct index_sync_fixture* fixture, case_t c) {
  SP_UNUSED(fixture);

  ctx_t* harness = ctx_get();
  sp_str_t tmp = tmpfs_get(&harness->fs, sp_str_view(c.name));

  sp_str_t remote = sp_fs_join_path(tmp, sp_str_lit("remote/index"));
  sp_str_t cache = sp_fs_join_path(tmp, sp_str_lit("cache/index"));

  sp_fs_create_dir(remote);
  git_repo_init(remote);

  // Always add an initial commit
  modify_index(remote);

  // Optionally sync
  if (c.setup != INDEX_SYNC_SETUP_REMOTE_ONLY) {
    spn_index_t setup_index = {
      .url = remote,
      .location = cache,
      .protocol = SPN_INDEX_PROTOCOL_GIT,
    };
    EXPECT_EQ(spn_index_sync(&setup_index), SPN_OK);
  }

  // Optionally add another commit
  if (c.mutate_remote) {
    modify_index(remote);
  }

  spn_index_t index = {
    .name = sp_str_lit("test"),
    .url = remote,
    .location = cache,
    .protocol = c.protocol,
  };

  EXPECT_EQ(spn_index_sync(&index), c.expect.result);
  EXPECT_EQ(sp_fs_exists(cache), c.expect.cached);
}

UTEST_F_SETUP(index_sync_fixture) {
  uf->unused = 0;

  ctx_t* harness = ctx_get();
  sp_context_push_allocator(sp_mem_arena_as_allocator(harness->arena));
}

UTEST_F_TEARDOWN(index_sync_fixture) {
  sp_context_pop();
}

UTEST_F(index_sync_fixture, sync_clone_when_missing) {
  run_index_sync_case(utest_result, uf, (case_t) {
    .name = "sync_clone_when_missing",
    .protocol = SPN_INDEX_PROTOCOL_GIT,
    .setup = INDEX_SYNC_SETUP_REMOTE_ONLY,
    .expect = {
      .result = SPN_OK,
      .cached = true,
    },
  });
}

UTEST_F(index_sync_fixture, sync_fetch_when_present) {
  run_index_sync_case(utest_result, uf, (case_t) {
    .name = "sync_fetch_when_present",
    .protocol = SPN_INDEX_PROTOCOL_GIT,
    .setup = INDEX_SYNC_SETUP_CLONED_REPO,
    .mutate_remote = true,
    .expect = {
      .result = SPN_OK,
      .cached = true,
    },
  });
}

UTEST_F(index_sync_fixture, sync_http_unimplemented) {
  run_index_sync_case(utest_result, uf, (case_t) {
    .name = "sync_http_unimplemented",
    .protocol = SPN_INDEX_PROTOCOL_HTTP,
    .setup = INDEX_SYNC_SETUP_REMOTE_ONLY,
    .expect = {
      .result = SPN_ERROR,
      .cached = false,
    },
  });
}
