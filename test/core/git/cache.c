#include "sp.h"
#include "utest.h"
#include "test.h"

#include "git/cache.h"
#include "git/patch.h"


/////////////////
// DESCRIPTOR //
////////////////
typedef struct {
  const c8* file;
  const c8* content;
} expect_file_t;

#define CACHE_TEST_MAX_PATCHES 4

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
  expect_file_t files[8];
  bool err;
} checkout_expect_t;

typedef struct {
  git_repo_fixture_t repo;
  checkout_req_t checkouts[4];
  struct {
    u32 dbs;
    u32 checkouts;
    checkout_expect_t detail[4];
  } expect;
} fixture_t;


///////////
// STATE //
///////////
struct git_cache {
  u32 dummy;
};

UTEST_F_SETUP(git_cache) {
}

UTEST_F_TEARDOWN(git_cache) {
}


///////////////
// EXECUTOR //
//////////////
static spn_git_checkout_id_t build_id(sp_mem_t mem, tmpfs_t* fs, const c8* name, git_repo_result_t* repo, checkout_req_t* req, u32 index) {
  u32 rev_idx = sp_parse_u32(sp_str_view(req->rev));
  spn_git_checkout_id_t id = {
    .url = repo->path,
    .rev = repo->commits[rev_idx],
    .dir = req->dir ? sp_str_view(req->dir) : SP_LIT(""),
  };

  if (req->patches[0].file) {
    sp_da(sp_str_t) files = sp_da_new(mem, sp_str_t);
    sp_carr_for(req->patches, jt) {
      patch_edit_t* edit = &req->patches[jt];
      if (!edit->file) {
        break;
      }
      sp_str_t text = sp_fmt(mem, "--- a/{}\n+++ b/{}\n@@ -1 +1 @@\n-{}+{}",
        sp_fmt_cstr(edit->file), sp_fmt_cstr(edit->file),
        sp_fmt_cstr(edit->from), sp_fmt_cstr(edit->to)).value;
      sp_str_t path = tmpfs_get(fs, sp_fmt(mem, "{}_patch_{}_{}.patch", sp_fmt_cstr(name), sp_fmt_uint(index), sp_fmt_uint(jt)).value);
      sp_fs_create_file_str(path, text);
      sp_da_push(files, path);
    }
    id.patches.files = files;
    u32 missing = 0;
    spn_git_patch_set_hash(&id.patches, &missing);
  }

  return id;
}

static void run_fixture(s32* utest_result, fixture_t fixture) {
  ctx_t* harness = ctx_get();
  sp_mem_t mem = sp_mem_arena_as_allocator(harness->arena);

  // build git repo fixture as "remote"
  git_repo_result_t repo = git_repo_build(&harness->fs, fixture.repo.name, &fixture.repo);

  // init cache in tmpfs
  sp_str_t cache_root = tmpfs_get(&harness->fs,
    sp_fmt(mem, "{}_cache", sp_fmt_cstr(fixture.repo.name)).value);
  sp_fs_create_dir(cache_root);

  spn_git_cache_t cache = sp_zero;
  spn_git_cache_init(&cache, mem, SP_NULLPTR, cache_root);

  // for each checkout request: ensure db, ensure rev, ensure checkout
  sp_carr_for(fixture.checkouts, it) {
    checkout_req_t* req = &fixture.checkouts[it];
    if (!req->rev) {
      break;
    }

    checkout_expect_t* expect = &fixture.expect.detail[it];
    spn_git_checkout_id_t id = build_id(mem, &harness->fs, fixture.repo.name, &repo, req, (u32)it);
    if (req->patches[0].file) {
      ASSERT_NE(id.patches.hash, (sp_hash_t)0);
    }

    spn_git_db_t* db = SP_NULLPTR;
    spn_err_t err = spn_git_cache_ensure_db(&cache, id.url, &db);
    ASSERT_EQ(err, SPN_OK);
    ASSERT_NE(db, SP_NULLPTR);

    err = spn_git_db_ensure_rev(db, id.rev);
    ASSERT_EQ(err, SPN_OK);

    spn_git_checkout_t* checkout = SP_NULLPTR;
    err = spn_git_cache_ensure_checkout(&cache, id, &checkout);
    ASSERT_NE(checkout, SP_NULLPTR);

    if (expect->err) {
      EXPECT_NE(err, SPN_OK);
      EXPECT_FALSE(sp_str_empty(checkout->error));
      EXPECT_FALSE(sp_fs_is_dir(checkout->path));
    }
    else {
      ASSERT_EQ(err, SPN_OK);
    }
  }

  // assert db and checkout counts
  EXPECT_EQ(fixture.expect.dbs, sp_ht_size(cache.db.entries));
  EXPECT_EQ(fixture.expect.checkouts, sp_str_om_size(cache.checkouts.entries));

  // verify each checkout's expected files on disk
  sp_carr_for(fixture.checkouts, it) {
    checkout_expect_t* expect = &fixture.expect.detail[it];
    if (!fixture.checkouts[it].rev) {
      break;
    }
    if (expect->err || !expect->files[0].file) {
      continue;
    }

    spn_git_checkout_id_t id = build_id(mem, &harness->fs, fixture.repo.name, &repo, &fixture.checkouts[it], (u32)it);

    spn_git_checkout_t* checkout = SP_NULLPTR;
    spn_git_cache_ensure_checkout(&cache, id, &checkout);
    ASSERT_NE(checkout, SP_NULLPTR);
    EXPECT_TRUE(sp_fs_is_dir(checkout->path));

    sp_carr_for(expect->files, f) {
      expect_file_t* file = &expect->files[f];
      if (!file->file) {
        break;
      }

      sp_str_t path = sp_fs_join_path(mem, checkout->path, sp_str_view(file->file));
      EXPECT_TRUE(sp_fs_exists(path));

      if (sp_fs_exists(path)) {
        sp_str_t content = test_read_file(mem, path);
        SP_EXPECT_STR_EQ_CSTR(content, file->content);
      }
    }
  }
}


////////////
// CASES //
///////////
UTEST_F(git_cache, single_full_repo) {
  run_fixture(utest_result, (fixture_t) {
    .repo = {
      .name = "single_full",
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
  });
}

UTEST_F(git_cache, monorepo_two_subdirs) {
  run_fixture(utest_result, (fixture_t) {
    .repo = {
      .name = "monorepo",
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
  });
}

UTEST_F(git_cache, different_commits) {
  run_fixture(utest_result, (fixture_t) {
    .repo = {
      .name = "versioned",
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
  });
}

UTEST_F(git_cache, patched_checkout_coexists_with_pristine) {
  run_fixture(utest_result, (fixture_t) {
    .repo = {
      .name = "patched",
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
  });
}

UTEST_F(git_cache, patches_apply_in_order) {
  run_fixture(utest_result, (fixture_t) {
    .repo = {
      .name = "ordered",
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
  });
}

UTEST_F(git_cache, patches_apply_at_repo_root_before_subdir) {
  run_fixture(utest_result, (fixture_t) {
    .repo = {
      .name = "patched_monorepo",
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
  });
}

UTEST_F(git_cache, patch_conflict_fails_without_checkout) {
  run_fixture(utest_result, (fixture_t) {
    .repo = {
      .name = "conflicted",
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
  });
}

UTEST_F(git_cache, patched_idempotent_ensure) {
  run_fixture(utest_result, (fixture_t) {
    .repo = {
      .name = "patched_idempotent",
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
  });
}

UTEST_F(git_cache, idempotent_ensure) {
  // requesting the same checkout twice yields the same result
  run_fixture(utest_result, (fixture_t) {
    .repo = {
      .name = "idempotent",
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
  });
}
