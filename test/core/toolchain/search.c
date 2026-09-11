#include "toolchain.h"
#include "toolchain/search.h"
#include "paths/paths.h"

#define SEARCH_MAX_DIRS 2
#define SEARCH_MAX_FILES 2

typedef struct {
  const c8* name;
  spn_os_t os;
  const c8* path;
  const c8* expect [SEARCH_MAX_DIRS];
} dirs_t;

static const dirs_t dirs_tests [] = {
  { "posix_single",              SPN_OS_LINUX,   "/X",            { "/X" } },
  { "posix_two",                 SPN_OS_LINUX,   "/X:/Y",         { "/X", "/Y" } },
  { "windows",                   SPN_OS_WINDOWS, "C:/X;C:/Y",     { "C:/X", "C:/Y" } },
  { "windows_backslashes",       SPN_OS_WINDOWS, "C:\\X\\Y",      { "C:/X/Y" } },
  { "windows_drive_relative",    SPN_OS_WINDOWS, "X;C:X;C:/Y",    { "C:/Y" } },
  { "empty_entries_dropped",     SPN_OS_LINUX,   ":/X::/Y:",      { "/X", "/Y" } },
  { "relative_entries_dropped",  SPN_OS_LINUX,   "X:/Y:Z/W",      { "/Y" } },
  { "trailing_separator",        SPN_OS_LINUX,   "/X/",           { "/X" } },
  { "trailing_separators",       SPN_OS_LINUX,   "/X//",          { "/X" } },
  { "malformed_entries_dropped", SPN_OS_LINUX,   "/X//Y:/./Z:/W", { "/W" } },
  { "empty",                     SPN_OS_LINUX,   "" },
};

typedef enum {
  SEARCH_NAME,
  SEARCH_FILE,
} program_kind_t;

typedef struct {
  const c8* name;
  spn_os_t os;
  program_kind_t kind;
  const c8* program;
  const c8* files [SEARCH_MAX_FILES];
  const c8* dirs [SEARCH_MAX_DIRS];
  const c8* expect;
} program_t;

static const program_t program_tests [] = {
  { "found_in_first_dir",  SPN_OS_LINUX,   SEARCH_NAME, "A",   .files = { "X/A", "Y/A" },     .dirs = { "X", "Y" }, .expect = "X/A" },
  { "found_in_later_dir",  SPN_OS_LINUX,   SEARCH_NAME, "A",   .files = { "Y/A" },            .dirs = { "X", "Y" }, .expect = "Y/A" },
  { "missing",             SPN_OS_LINUX,   SEARCH_NAME, "A",   .files = { "Y/B" },            .dirs = { "X", "Y" } },
  { "windows_suffix",      SPN_OS_WINDOWS, SEARCH_NAME, "A",   .files = { "X/A.exe" },        .dirs = { "X" },      .expect = "X/A.exe" },
  { "windows_bare_first",  SPN_OS_WINDOWS, SEARCH_NAME, "A",   .files = { "X/A", "X/A.exe" }, .dirs = { "X" },      .expect = "X/A" },
  { "file_found",          SPN_OS_LINUX,   SEARCH_FILE, "X/A", .files = { "X/A" },            .expect = "X/A" },
  { "file_missing",        SPN_OS_LINUX,   SEARCH_FILE, "X/A", .files = { "Y/A" } },
  { "file_windows_suffix", SPN_OS_WINDOWS, SEARCH_FILE, "X/A", .files = { "X/A.exe" },        .expect = "X/A.exe" },
};

static u32 count(const c8* const* strs, u32 max) {
  u32 n = 0;
  while (n < max && strs[n]) {
    n++;
  }
  return n;
}

sp_test_each(search, dirs, dirs_t, dirs_tests) {
  sp_mem_t mem = sp_test_arena(t);
  u32 num_expect = count(it->expect, SEARCH_MAX_DIRS);
  sp_da(sp_str_t) dirs = spn_search_dirs(spn_search_rules(it->os), mem, sp_cstr_as_str(it->path));
  sp_expect_eq(t, sp_da_size(dirs), num_expect);
  sp_for(at, sp_min(sp_da_size(dirs), num_expect)) {
    sp_expect_str_eq_c(t, dirs[at], it->expect[at]);
  }
  return SP_OK;
}

sp_test_each(search, program, program_t, program_tests) {
  sp_mem_t mem = sp_test_arena(t);
  sp_str_t root = sp_test_dir(t);
  sp_fs_create_dir(sp_fs_join_path(mem, root, sp_str_lit("X")));
  sp_fs_create_dir(sp_fs_join_path(mem, root, sp_str_lit("Y")));
  sp_carr_for_until(it->files, at, it->files[at]) {
    sp_fs_create_file_str(sp_fs_join_path(mem, root, sp_cstr_as_str(it->files[at])), sp_str_lit("A"));
  }
  sp_da(sp_str_t) dirs = sp_da_new(mem, sp_str_t);
  sp_carr_for_until(it->dirs, at, it->dirs[at]) {
    sp_da_push(dirs, sp_fs_join_path(mem, root, sp_cstr_as_str(it->dirs[at])));
  }

  spn_path_roots_t roots = sp_zero;
  spn_arg_t program = sp_zero;
  switch (it->kind) {
    case SEARCH_NAME: {
      program = spn_arg_lit(sp_cstr_as_str(it->program));
      break;
    }
    case SEARCH_FILE: {
      program = spn_arg_path(spn_path_make(&roots, sp_fs_join_path(mem, root, sp_cstr_as_str(it->program))));
      break;
    }
  }
  sp_str_t found = spn_search_program(spn_search_rules(it->os), mem, &roots, program, dirs);

  sp_str_t expect = it->expect ? sp_fs_join_path(mem, root, sp_cstr_as_str(it->expect)) : sp_str_lit("");
  sp_expect_str_eq(t, found, expect);
  return SP_OK;
}
