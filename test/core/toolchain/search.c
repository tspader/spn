#include "toolchain.h"
#include "toolchain/search.h"

#define SEARCH_MAX_SPLIT 3

#if defined(SP_WIN32)
  #define SEARCH_SEP ";"
#else
  #define SEARCH_SEP ":"
#endif

typedef struct {
  const c8* name;
  const c8* path;
  const c8* expect [SEARCH_MAX_SPLIT];
} split_t;

static const split_t split_tests [] = {
  { "single", "A", { "A" } },
  { "two", "A" SEARCH_SEP "B", { "A", "B" } },
  { "empty_entries_dropped", SEARCH_SEP "A" SEARCH_SEP SEARCH_SEP "B" SEARCH_SEP, { "A", "B" } },
  { "empty", "" },
};

sp_test_each(search, split_path, split_t, split_tests) {
  sp_da(sp_str_t) dirs = spn_search_split_path(sp_test_arena(t), sp_cstr_as_str(it->path));
  sp_must_strs_eq(t, dirs, sp_da_size(dirs), it->expect);
  return SP_OK;
}

typedef struct {
  const c8* name;
  const c8* program;
  bool relative;
  const c8* files [2];
  const c8* dirs [2];
  const c8* expect;
} program_t;

static const program_t program_tests [] = {
  { "found_in_first_dir",       "A",   .files = { "X/A", "Y/A" }, .dirs = { "X", "Y" }, .expect = "X/A" },
  { "found_in_later_dir",       "A",   .files = { "Y/A" },        .dirs = { "X", "Y" }, .expect = "Y/A" },
  { "missing",                  "A",   .files = { "Y/B" },        .dirs = { "X", "Y" } },
  { "path_bypasses_dirs",       "Y/A", .files = { "Y/A" },        .dirs = { "X" },      .expect = "Y/A" },
  { "path_missing",             "Y/A", .files = { "X/A" },        .dirs = { "X" } },
  { "relative_path_is_rooted",  "Y/A", .relative = true, .files = { "Y/A" }, .dirs = { "X" }, .expect = "Y/A" },
  { "relative_dir_is_rooted",   "A",   .relative = true, .files = { "Y/A" }, .dirs = { "Y" }, .expect = "Y/A" },
};

sp_test_each(search, program, program_t, program_tests) {
  sp_mem_t mem = sp_test_arena(t);
  sp_str_t root = sp_test_dir(t);
  sp_fs_create_dir(sp_fs_join_path(mem, root, sp_str_lit("X")));
  sp_fs_create_dir(sp_fs_join_path(mem, root, sp_str_lit("Y")));
  sp_carr_for(it->files, at) {
    if (!it->files[at]) {
      break;
    }
    sp_fs_create_file_str(sp_fs_join_path(mem, root, sp_cstr_as_str(it->files[at])), sp_str_lit("A"));
  }
  sp_da(sp_str_t) dirs = sp_da_new(mem, sp_str_t);
  sp_carr_for(it->dirs, at) {
    if (!it->dirs[at]) {
      break;
    }
    sp_str_t dir = sp_cstr_as_str(it->dirs[at]);
    sp_da_push(dirs, it->relative ? dir : sp_fs_join_path(mem, root, dir));
  }

  sp_str_t program = sp_cstr_as_str(it->program);
  if (!it->relative && sp_str_find_c8(program, '/') >= 0) {
    program = sp_fs_join_path(mem, root, program);
  }
  sp_str_t expect = it->expect ? sp_fs_join_path(mem, root, sp_cstr_as_str(it->expect)) : sp_str_lit("");
  sp_expect_str_eq(t, spn_search_program(mem, root, program, dirs), expect);
  return SP_OK;
}
