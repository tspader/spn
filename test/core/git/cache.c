#include "git.h"

#define CACHE_TEST_MAX_PATCHES   4
#define CACHE_TEST_MAX_CHECKOUTS 4

typedef struct {
  const c8* file;
  const c8* content;
} expect_file_t;

typedef struct {
  const c8* file;
  const c8* from;
  const c8* to;
} patch_edit_t;

typedef struct {
  const c8* rev;
  const c8* dir;
  patch_edit_t patches [CACHE_TEST_MAX_PATCHES];
} checkout_req_t;

typedef struct {
  expect_file_t files [8];
  bool err;
} checkout_expect_t;

typedef struct {
  u32 dbs;
  u32 checkouts;
  checkout_expect_t detail [CACHE_TEST_MAX_CHECKOUTS];
} cache_expect_t;

typedef struct {
  const c8* name;
  git_repo_fixture_t repo;
  checkout_req_t checkouts [CACHE_TEST_MAX_CHECKOUTS];
  cache_expect_t expect;
} cache_test_t;

static const cache_test_t tests [] = {
  {
    .name = "single_full_repo",
    .repo = {
      .commits = {
        {
          .message = "initial",
          .files = {
            { "spn.toml", r("[package]") "name = \"spum\"" },
            { "src/lib.c", "int add(int a, int b) { return a + b; }" },
          },
        },
      },
    },
    .checkouts = {
      { .rev = "0" },
    },
    .expect = {
      .dbs = 1,
      .checkouts = 1,
      .detail = {
        {
          .files = {
            { .file = "spn.toml", .content = r("[package]") "name = \"spum\"" },
            { .file = "src/lib.c", .content = "int add(int a, int b) { return a + b; }" },
          },
        },
      },
    },
  },
  {
    .name = "monorepo_two_subdirs",
    .repo = {
      .commits = {
        {
          .message = "initial",
          .files = {
            { "packages/math/spn.toml", r("[package]") "name = \"math\"" },
            { "packages/math/src/math.c", "int add(int a, int b) { return a + b; }" },
            { "packages/audio/spn.toml", r("[package]") "name = \"audio\"" },
            { "packages/audio/src/audio.c", "void init() {}" },
          },
        },
      },
    },
    .checkouts = {
      { .rev = "0", .dir = "packages/math" },
      { .rev = "0", .dir = "packages/audio" },
    },
    .expect = {
      .dbs = 1,
      .checkouts = 2,
      .detail = {
        {
          .files = {
            { .file = "spn.toml", .content = r("[package]") "name = \"math\"" },
            { .file = "src/math.c", .content = "int add(int a, int b) { return a + b; }" },
          },
        },
        {
          .files = {
            { .file = "spn.toml", .content = r("[package]") "name = \"audio\"" },
            { .file = "src/audio.c", .content = "void init() {}" },
          },
        },
      },
    },
  },
  {
    .name = "different_commits",
    .repo = {
      .commits = {
        {
          .message = "v1",
          .files = {
            { "lib.h", "#define VERSION 1" },
          },
        },
        {
          .message = "v2",
          .files = {
            { "lib.h", "#define VERSION 2" },
          },
        },
      },
    },
    .checkouts = {
      { .rev = "0" },
      { .rev = "1" },
    },
    .expect = {
      .dbs = 1,
      .checkouts = 2,
      .detail = {
        {
          .files = {
            { .file = "lib.h", .content = "#define VERSION 1" },
          },
        },
        {
          .files = {
            { .file = "lib.h", .content = "#define VERSION 2" },
          },
        },
      },
    },
  },
  {
    .name = "patched_checkout_coexists_with_pristine",
    .repo = {
      .commits = {
        {
          .message = "initial",
          .files = {
            { "F", "1\n" },
          },
        },
      },
    },
    .checkouts = {
      { .rev = "0" },
      {
        .rev = "0",
        .patches = {
          { .file = "F", .from = "1\n", .to = "2\n" },
        },
      },
    },
    .expect = {
      .dbs = 1,
      .checkouts = 2,
      .detail = {
        {
          .files = {
            { .file = "F", .content = "1\n" },
          },
        },
        {
          .files = {
            { .file = "F", .content = "2\n" },
          },
        },
      },
    },
  },
  {
    .name = "patches_apply_in_order",
    .repo = {
      .commits = {
        {
          .message = "initial",
          .files = {
            { "F", "1\n" },
          },
        },
      },
    },
    .checkouts = {
      {
        .rev = "0",
        .patches = {
          { .file = "F", .from = "1\n", .to = "2\n" },
          { .file = "F", .from = "2\n", .to = "3\n" },
        },
      },
    },
    .expect = {
      .dbs = 1,
      .checkouts = 1,
      .detail = {
        {
          .files = {
            { .file = "F", .content = "3\n" },
          },
        },
      },
    },
  },
  {
    .name = "patches_apply_at_repo_root_before_subdir",
    .repo = {
      .commits = {
        {
          .message = "initial",
          .files = {
            { "M/F", "1\n" },
            { "A/F", "1\n" },
          },
        },
      },
    },
    .checkouts = {
      {
        .rev = "0",
        .dir = "M",
        .patches = {
          { .file = "M/F", .from = "1\n", .to = "2\n" },
        },
      },
    },
    .expect = {
      .dbs = 1,
      .checkouts = 1,
      .detail = {
        {
          .files = {
            { .file = "F", .content = "2\n" },
          },
        },
      },
    },
  },
  {
    .name = "patch_conflict_fails_without_checkout",
    .repo = {
      .commits = {
        {
          .message = "initial",
          .files = {
            { "F", "1\n" },
          },
        },
      },
    },
    .checkouts = {
      {
        .rev = "0",
        .patches = {
          { .file = "F", .from = "9\n", .to = "8\n" },
        },
      },
    },
    .expect = {
      .dbs = 1,
      .checkouts = 1,
      .detail = {
        { .err = true },
      },
    },
  },
  {
    .name = "patched_idempotent_ensure",
    .repo = {
      .commits = {
        {
          .message = "initial",
          .files = {
            { "F", "1\n" },
          },
        },
      },
    },
    .checkouts = {
      {
        .rev = "0",
        .patches = {
          { .file = "F", .from = "1\n", .to = "2\n" },
        },
      },
      {
        .rev = "0",
        .patches = {
          { .file = "F", .from = "1\n", .to = "2\n" },
        },
      },
    },
    .expect = {
      .dbs = 1,
      .checkouts = 1,
      .detail = {
        {
          .files = {
            { .file = "F", .content = "2\n" },
          },
        },
      },
    },
  },
  // requesting the same checkout twice yields the same result
  {
    .name = "idempotent_ensure",
    .repo = {
      .commits = {
        {
          .message = "initial",
          .files = {
            { "data.txt", "hello" },
          },
        },
      },
    },
    .checkouts = {
      { .rev = "0" },
      { .rev = "0" },
    },
    .expect = {
      .dbs = 1,
      .checkouts = 1,
      .detail = {
        {
          .files = {
            { .file = "data.txt", .content = "hello" },
          },
        },
      },
    },
  },
};

static spn_git_checkout_id_t build_id(sp_test_t* t, git_repo_result_t* repo, const checkout_req_t* req, u32 index) {
  sp_mem_t mem = sp_test_arena(t);
  u32 rev_idx = sp_parse_u32(sp_str_view(req->rev));
  spn_git_checkout_id_t id = {
    .url = repo->path,
    .rev = repo->commits[rev_idx],
    .dir = req->dir ? sp_str_view(req->dir) : SP_LIT(""),
  };

  if (req->patches[0].file) {
    sp_da(sp_str_t) files = sp_da_new(mem, sp_str_t);
    sp_carr_for(req->patches, jt) {
      const patch_edit_t* edit = &req->patches[jt];
      if (!edit->file) {
        break;
      }
      sp_str_t text = sp_fmt(mem, "--- a/{}\n+++ b/{}\n@@ -1 +1 @@\n-{}+{}",
        sp_fmt_cstr(edit->file), sp_fmt_cstr(edit->file),
        sp_fmt_cstr(edit->from), sp_fmt_cstr(edit->to)).value;
      sp_str_t path = sp_fs_join_path(mem, sp_test_dir(t),
        sp_fmt(mem, "patch_{}_{}.patch", sp_fmt_uint(index), sp_fmt_uint(jt)).value);
      sp_fs_create_file_str(path, text);
      sp_da_push(files, path);
    }
    id.patches.files = files;
    u32 missing = 0;
    spn_git_patch_set_hash(&id.patches, &missing);
  }

  return id;
}

sp_test_each(git_cache, ensure, cache_test_t, tests) {
  sp_mem_t mem = sp_test_arena(t);

  // build git repo fixture as "remote"
  git_repo_result_t repo = git_repo_build_at(sp_test_dir(t), "R", &it->repo);

  sp_str_t cache_root = sp_fs_join_path(mem, sp_test_dir(t), sp_str_lit("cache"));
  sp_fs_create_dir(cache_root);

  spn_git_cache_t cache = sp_zero;
  spn_git_cache_init(&cache, mem, SP_NULLPTR, cache_root);

  u32 num_checkouts = 0;
  sp_carr_detect_len(it->checkouts, num_checkouts, it->checkouts[num_checkouts].rev);

  spn_git_checkout_id_t ids [CACHE_TEST_MAX_CHECKOUTS] = sp_zero;
  sp_for(c, num_checkouts) {
    ids[c] = build_id(t, &repo, &it->checkouts[c], (u32)c);
    if (it->checkouts[c].patches[0].file) {
      sp_must_ne(t, ids[c].patches.hash, (sp_hash_t)0);
    }
  }

  // for each checkout request: ensure db, ensure rev, ensure checkout
  sp_for(c, num_checkouts) {
    checkout_expect_t* expect = &it->expect.detail[c];

    spn_git_db_t* db = SP_NULLPTR;
    spn_err_t err = spn_git_cache_ensure_db(&cache, ids[c].url, &db);
    sp_must_eq(t, err, SPN_OK);
    sp_must(t, db != SP_NULLPTR);

    err = spn_git_db_ensure_rev(db, ids[c].rev);
    sp_must_eq(t, err, SPN_OK);

    spn_git_checkout_t* checkout = SP_NULLPTR;
    err = spn_git_cache_ensure_checkout(&cache, ids[c], &checkout);
    sp_must(t, checkout != SP_NULLPTR);

    if (expect->err) {
      sp_expect(t, err != SPN_OK);
      sp_expect(t, !sp_str_empty(checkout->error));
      sp_expect(t, !sp_fs_is_dir(checkout->path));
    }
    else {
      sp_must_eq(t, err, SPN_OK);
    }
  }

  // assert db and checkout counts
  sp_expect_eq(t, it->expect.dbs, sp_ht_size(cache.db.entries));
  sp_expect_eq(t, it->expect.checkouts, sp_str_om_size(cache.checkouts.entries));

  // verify each checkout's expected files on disk
  sp_for(c, num_checkouts) {
    checkout_expect_t* expect = &it->expect.detail[c];
    if (expect->err || !expect->files[0].file) {
      continue;
    }

    spn_git_checkout_t* checkout = SP_NULLPTR;
    spn_git_cache_ensure_checkout(&cache, ids[c], &checkout);
    sp_must(t, checkout != SP_NULLPTR);
    sp_expect(t, sp_fs_is_dir(checkout->path));

    sp_carr_for(expect->files, f) {
      expect_file_t* file = &expect->files[f];
      if (!file->file) {
        break;
      }

      sp_str_t path = sp_fs_join_path(mem, checkout->path, sp_str_view(file->file));
      sp_must(t, sp_fs_exists(path));
      sp_expect_str_eq_c(t, test_read_file(mem, path), file->content);
    }
  }

  return SP_OK;
}
