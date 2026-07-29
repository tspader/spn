#include "git.h"

#define PATCH_TEST_MAX_FILES 4

typedef struct {
  spn_err_t err;
  const c8* missing;
  u32 missing_index;
  bool hash_matches_pair;
} expect_t;

typedef struct {
  const c8* name;
  const c8* files [PATCH_TEST_MAX_FILES];
  const c8* pair [PATCH_TEST_MAX_FILES];
  expect_t expect;
} patch_load_t;

static const patch_load_t tests [] = {
  // same contents at different paths -> same hash: content keys identity, not paths
  {
    .name = "hash_ignores_paths",
    .files = { "A", "B" },
    .pair = { "A", "B" },
    .expect = { .hash_matches_pair = true },
  },
  // different content -> different hash
  {
    .name = "hash_tracks_content",
    .files = { "A" },
    .pair = { "B" },
    .expect = { .hash_matches_pair = false },
  },
  // same contents, reversed order -> different hash: order is part of identity
  {
    .name = "hash_tracks_order",
    .files = { "A", "B" },
    .pair = { "B", "A" },
    .expect = { .hash_matches_pair = false },
  },
  // a missing file fails the load and reports which file
  {
    .name = "missing_file",
    .files = { "A" },
    .expect = { .err = SPN_ERROR, .missing = "nonexistent.patch", .missing_index = 1 },
  },
};

static sp_da(sp_str_t) write_patch_files(sp_test_t* t, const c8* tag, const c8* const* contents, u32 cap) {
  sp_mem_t mem = sp_test_arena(t);
  sp_da(sp_str_t) files = sp_da_new(mem, sp_str_t);
  sp_for(i, cap) {
    if (!contents[i]) {
      break;
    }
    sp_str_t path = sp_fs_join_path(mem, sp_test_dir(t),
      sp_fmt(mem, "{}_{}.patch", sp_fmt_cstr(tag), sp_fmt_uint(i)).value);
    sp_fs_create_file_cstr(path, contents[i]);
    sp_da_push(files, path);
  }
  return files;
}

sp_test_each(git_patch, set_hash, patch_load_t, tests) {
  sp_mem_t mem = sp_test_arena(t);

  sp_da(sp_str_t) files = write_patch_files(t, "a", it->files, PATCH_TEST_MAX_FILES);
  if (it->expect.missing) {
    sp_da_push(files, sp_fs_join_path(mem, sp_test_dir(t), sp_str_view(it->expect.missing)));
  }

  spn_git_patch_set_t set = { .files = files };
  u32 missing = 0;
  spn_err_t err = spn_git_patch_set_hash(&set, &missing);
  sp_expect_eq(t, err, it->expect.err);

  if (it->expect.err) {
    sp_expect_eq(t, missing, it->expect.missing_index);
    sp_expect_eq(t, set.hash, (sp_hash_t)0);
    return SP_OK;
  }

  sp_expect_ne(t, set.hash, (sp_hash_t)0);

  if (it->pair[0]) {
    sp_da(sp_str_t) pair_files = write_patch_files(t, "b", it->pair, PATCH_TEST_MAX_FILES);
    spn_git_patch_set_t pair = { .files = pair_files };
    spn_err_t pair_err = spn_git_patch_set_hash(&pair, &missing);
    sp_expect_eq(t, pair_err, SPN_OK);
    sp_expect_eq(t, it->expect.hash_matches_pair, set.hash == pair.hash);
  }

  return SP_OK;
}
