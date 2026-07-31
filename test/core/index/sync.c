#include "index.h"

typedef struct {
  const c8* name;

  spn_index_protocol_t protocol;
  bool cloned;
  bool mutate_remote;

  struct {
    spn_err_t result;
    bool cached;
  } expect;
} sync_test_t;

static const sync_test_t tests [] = {
  {
    .name = "clone_when_missing",
    .expect = {
      .cached = true,
    },
  },
  {
    .name = "fetch_when_present",
    .cloned = true,
    .mutate_remote = true,
    .expect = {
      .cached = true,
    },
  },
  {
    .name = "http_unimplemented",
    .protocol = SPN_INDEX_PROTOCOL_HTTP,
    .expect = {
      .result = SPN_ERROR,
    },
  },
};

static void commit_seed(sp_mem_t mem, sp_str_t repo, const c8* name, u32 seq) {
  sp_str_t file = sp_fs_join_path(mem, repo, sp_str_lit("seed.txt"));
  sp_fs_create_file_str(file, sp_fmt(mem, "{}-{}", sp_fmt_cstr(name), sp_fmt_uint(seq)).value);

  git_repo_stage_all(repo);
  git_repo_commit(repo, sp_str_lit("seed"));
}

typedef struct {
  const c8* name;
  bool exists;

  struct {
    spn_err_t result;
  } expect;
} dir_sync_test_t;

static const dir_sync_test_t dir_tests [] = {
  {
    .name = "present",
    .exists = true,
  },
  {
    .name = "missing",
    .expect = {
      .result = SPN_ERROR,
    },
  },
};

sp_test_each(index_sync, dir, dir_sync_test_t, dir_tests) {
  sp_mem_t mem = sp_test_arena(t);

  sp_str_t location = sp_fs_join_path(mem, sp_test_dir(t), sp_str_lit("index"));
  if (it->exists) {
    sp_fs_create_dir(location);
  }

  spn_index_info_t index = {
    .location = location,
    .protocol = SPN_INDEX_PROTOCOL_DIR,
  };

  sp_expect_eq(t, it->expect.result, spn_index_sync(&index, false));
  if (it->exists) {
    sp_expect_eq(t, false, spn_index_needs_fetch(&index));
  }

  return SP_OK;
}

sp_test_each(index_sync, sync, sync_test_t, tests) {
  sp_mem_t mem = sp_test_arena(t);
  sp_str_t tmp = sp_test_dir(t);

  sp_str_t remote = sp_fs_join_path(mem, tmp, sp_str_lit("remote/index"));
  sp_str_t cache = sp_fs_join_path(mem, tmp, sp_str_lit("cache/index"));

  sp_fs_create_dir(remote);
  git_repo_init(remote);
  commit_seed(mem, remote, it->name, 0);

  spn_index_info_t index = {
    .name = sp_str_lit("test"),
    .protocol = it->protocol,
    .git = { .url = remote },
    .location = cache,
  };

  if (it->cloned) {
    sp_expect_eq(t, SPN_OK, spn_index_sync(&index, false));
  }
  if (it->mutate_remote) {
    commit_seed(mem, remote, it->name, 1);
  }

  sp_expect_eq(t, it->expect.result, spn_index_sync(&index, false));
  sp_expect_eq(t, it->expect.cached, sp_fs_exists(cache));

  return SP_OK;
}
