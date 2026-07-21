#include "sp.h"
#include "utest.h"
#include "test.h"

#include "git/patch.h"


/////////////////
// DESCRIPTOR //
////////////////
#define PATCH_TEST_MAX_FILES 4

typedef struct {
  spn_err_t err;
  const c8* missing;
  u32 missing_index;
  bool hash_matches_pair;
} patch_load_expect_t;

typedef struct {
  const c8* name;
  const c8* files [PATCH_TEST_MAX_FILES];
  const c8* pair [PATCH_TEST_MAX_FILES];
  patch_load_expect_t expect;
} patch_load_t;


///////////
// STATE //
///////////
struct git_patch {
  u32 dummy;
};

UTEST_F_SETUP(git_patch) {
}

UTEST_F_TEARDOWN(git_patch) {
}


///////////////
// EXECUTOR //
//////////////
static sp_da(sp_str_t) write_patch_files(sp_mem_t mem, tmpfs_t* fs, const c8* name, const c8* tag, const c8* const* contents, u32 cap) {
  sp_da(sp_str_t) files = sp_da_new(mem, sp_str_t);
  sp_for(it, cap) {
    if (!contents[it]) {
      break;
    }
    sp_str_t path = tmpfs_get(fs, sp_fmt(mem, "{}_{}_{}.patch", sp_fmt_cstr(name), sp_fmt_cstr(tag), sp_fmt_uint(it)).value);
    sp_fs_create_file_cstr(path, contents[it]);
    sp_da_push(files, path);
  }
  return files;
}

static void run_patch_load(s32* utest_result, patch_load_t t) {
  ctx_t* harness = ctx_get();
  sp_mem_t mem = sp_mem_arena_as_allocator(harness->arena);

  sp_da(sp_str_t) files = write_patch_files(mem, &harness->fs, t.name, "a", t.files, PATCH_TEST_MAX_FILES);
  if (t.expect.missing) {
    sp_da_push(files, tmpfs_get(&harness->fs, sp_str_view(t.expect.missing)));
  }

  spn_git_patch_set_t set = { .files = files };
  u32 missing = 0;
  spn_err_t err = spn_git_patch_set_hash(&set, &missing);
  EXPECT_EQ(err, t.expect.err);

  if (t.expect.err) {
    EXPECT_EQ(missing, t.expect.missing_index);
    EXPECT_EQ(set.hash, (sp_hash_t)0);
    return;
  }

  EXPECT_NE(set.hash, (sp_hash_t)0);

  if (t.pair[0]) {
    sp_da(sp_str_t) pair_files = write_patch_files(mem, &harness->fs, t.name, "b", t.pair, PATCH_TEST_MAX_FILES);
    spn_git_patch_set_t pair = { .files = pair_files };
    spn_err_t pair_err = spn_git_patch_set_hash(&pair, &missing);
    EXPECT_EQ(pair_err, SPN_OK);
    EXPECT_EQ(t.expect.hash_matches_pair, set.hash == pair.hash);
  }
}


////////////
// CASES //
///////////

// same contents at different paths -> same hash: content keys identity, not paths
UTEST_F(git_patch, hash_ignores_paths) {
  run_patch_load(utest_result, (patch_load_t) {
    .name = "paths",
    .files = { "A", "B" },
    .pair = { "A", "B" },
    .expect = { .hash_matches_pair = true },
  });
}

// different content -> different hash
UTEST_F(git_patch, hash_tracks_content) {
  run_patch_load(utest_result, (patch_load_t) {
    .name = "content",
    .files = { "A" },
    .pair = { "B" },
    .expect = { .hash_matches_pair = false },
  });
}

// same contents, reversed order -> different hash: order is part of identity
UTEST_F(git_patch, hash_tracks_order) {
  run_patch_load(utest_result, (patch_load_t) {
    .name = "order",
    .files = { "A", "B" },
    .pair = { "B", "A" },
    .expect = { .hash_matches_pair = false },
  });
}

// a missing file fails the load and reports which file
UTEST_F(git_patch, missing_file) {
  run_patch_load(utest_result, (patch_load_t) {
    .name = "missing",
    .files = { "A" },
    .expect = { .err = SPN_ERROR, .missing = "missing_nonexistent.patch", .missing_index = 1 },
  });
}
