#include "sp.h"
#include "utest.h"
#include "test.h"

#include "git/cache.h"


/////////////////
// DESCRIPTOR //
////////////////
typedef struct {
  const c8* file;
  const c8* content;
} expect_file_t;

typedef struct {
  const c8* rev;
  const c8* dir;
} checkout_req_t;

typedef struct {
  expect_file_t files[8];
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
  ctx_t* harness = ctx_get();
  sp_context_push_allocator(sp_mem_arena_as_allocator(harness->arena));
}

UTEST_F_TEARDOWN(git_cache) {
  sp_context_pop();
}


///////////////
// EXECUTOR //
//////////////
static spn_git_checkout_id_t build_id(git_repo_result_t* repo, checkout_req_t* req) {
  u32 rev_idx = sp_parse_u32(sp_str_view(req->rev));
  return (spn_git_checkout_id_t) {
    .url = repo->path,
    .rev = repo->commits[rev_idx],
    .dir = req->dir ? sp_str_view(req->dir) : SP_LIT(""),
  };
}

static void run_fixture(s32* utest_result, fixture_t fixture) {
  ctx_t* harness = ctx_get();

  // build git repo fixture as "remote"
  git_repo_result_t repo = git_repo_build(&harness->fs, fixture.repo.name, &fixture.repo);

  // init cache in tmpfs
  sp_str_t cache_root = tmpfs_get(&harness->fs,
    sp_format("{}_cache", SP_FMT_CSTR(fixture.repo.name)));
  sp_fs_create_dir(cache_root);

  spn_git_cache_t cache = SP_ZERO_INITIALIZE();
  spn_git_cache_init(&cache, cache_root);

  // for each checkout request: ensure db, ensure rev, ensure checkout
  sp_carr_for(fixture.checkouts, it) {
    checkout_req_t* req = &fixture.checkouts[it];
    if (!req->rev) {
      break;
    }

    spn_git_checkout_id_t id = build_id(&repo, req);

    spn_git_db_t* db = SP_NULLPTR;
    spn_err_t err = spn_git_cache_ensure_db(&cache, id.url, &db);
    ASSERT_EQ(err, SPN_OK);
    ASSERT_NE(db, SP_NULLPTR);

    err = spn_git_db_ensure_rev(db, id.rev);
    ASSERT_EQ(err, SPN_OK);

    spn_git_checkout_t* checkout = SP_NULLPTR;
    err = spn_git_cache_ensure_checkout(&cache, id, &checkout);
    ASSERT_EQ(err, SPN_OK);
    ASSERT_NE(checkout, SP_NULLPTR);
  }

  // assert db and checkout counts
  EXPECT_EQ(fixture.expect.dbs, sp_ht_size(cache.db.entries));
  EXPECT_EQ(fixture.expect.checkouts, sp_ht_size(cache.checkouts.entries));

  // verify each checkout's expected files on disk
  sp_carr_for(fixture.expect.detail, it) {
    checkout_expect_t* expect = &fixture.expect.detail[it];
    if (!expect->files[0].file) {
      break;
    }

    spn_git_checkout_id_t id = build_id(&repo, &fixture.checkouts[it]);

    spn_git_checkout_t* checkout = SP_NULLPTR;
    spn_git_cache_ensure_checkout(&cache, id, &checkout);
    ASSERT_NE(checkout, SP_NULLPTR);
    EXPECT_TRUE(sp_fs_is_dir(checkout->path));

    sp_carr_for(expect->files, f) {
      expect_file_t* file = &expect->files[f];
      if (!file->file) {
        break;
      }

      sp_str_t path = sp_fs_join_path(spn_allocator, checkout->path, sp_str_view(file->file));
      EXPECT_TRUE(sp_fs_exists(path));

      if (sp_fs_exists(path)) {
        sp_str_t content = sp_zero; sp_io_read_file(spn_allocator, path, &content);
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
