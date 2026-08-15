#include "spn_test.h"

#include "dag/wasi/canonicalize.h"

#define CANON_MAX_SETUP 8

typedef enum {
  CANON_SETUP_FILE,
  CANON_SETUP_DIR,
  CANON_SETUP_SYMLINK,
} canon_setup_kind_t;

typedef struct {
  const c8* path;
  canon_setup_kind_t kind;
  const c8* target;
} canon_setup_t;

typedef struct {
  bool set;
  bool empty;
  const c8* parent;
  const c8* leaf;
  bool exists;
} canon_expect_t;

typedef struct {
  const c8* name;
  canon_setup_t setup [CANON_MAX_SETUP];
  bool symlinks;
  const c8* input;
  canon_expect_t expect;
  canon_expect_t windows;
} canon_test_t;

static const canon_test_t canon_tests [] = {
  {
    .name = "existing_file",
    .setup = {
      { "A", CANON_SETUP_DIR },
      { "A/B" },
    },
    .input = "A/B",
    .expect = { .parent = "A/B", .exists = true },
  },
  {
    .name = "existing_dotdot_resolves",
    .setup = {
      { "A", CANON_SETUP_DIR },
      { "A/B", CANON_SETUP_DIR },
    },
    .input = "A/B/..",
    .expect = { .parent = "A", .exists = true },
  },
  {
    .name = "missing_leaf",
    .setup = {
      { "A", CANON_SETUP_DIR },
    },
    .input = "A/M",
    .expect = { .parent = "A", .leaf = "M" },
  },
  {
    .name = "missing_component_drops_tail",
    .setup = {
      { "A", CANON_SETUP_DIR },
    },
    .input = "A/M/D/E",
    .expect = { .parent = "A", .leaf = "M" },
  },
  {
    .name = "dotdot_after_missing_is_not_collapsed",
    .setup = {
      { "A", CANON_SETUP_DIR },
      { "A/X" },
    },
    .input = "A/M/../X",
    .expect = { .parent = "A", .leaf = "M" },
    .windows = { .set = true, .parent = "A/X", .exists = true },
  },
  {
    .name = "dotdot_over_existing_dir",
    .setup = {
      { "A", CANON_SETUP_DIR },
      { "A/B", CANON_SETUP_DIR },
    },
    .input = "A/B/../M",
    .expect = { .parent = "A", .leaf = "M" },
  },
  {
    .name = "dot_segment",
    .setup = {
      { "A", CANON_SETUP_DIR },
    },
    .input = "A/./M",
    .expect = { .parent = "A", .leaf = "M" },
  },
  {
    .name = "dotdot_chain_climbs_to_sandbox",
    .setup = {
      { "A", CANON_SETUP_DIR },
      { "A/B", CANON_SETUP_DIR },
    },
    .input = "A/B/../../M",
    .expect = { .parent = "", .leaf = "M" },
  },
  {
    .name = "file_in_the_middle",
    .setup = {
      { "F" },
    },
    .input = "F/M",
    .expect = { .parent = "F", .leaf = "M" },
  },
  {
    .name = "trailing_dot_on_file_degrades_to_prefix",
    .setup = {
      { "F" },
    },
    .input = "F/.",
    .expect = { .parent = "F", .exists = true },
  },
  {
    .name = "dotdot_through_symlink_is_physical",
    .setup = {
      { "T", CANON_SETUP_DIR },
      { "T/S", CANON_SETUP_DIR },
      { .path = "L", .kind = CANON_SETUP_SYMLINK, .target = "T/S" },
    },
    .symlinks = true,
    .input = "L/../M",
    .expect = { .parent = "T", .leaf = "M" },
    .windows = { .set = true, .parent = "", .leaf = "M" },
  },
  {
    .name = "symlink_resolves",
    .setup = {
      { "A" },
      { .path = "L", .kind = CANON_SETUP_SYMLINK, .target = "A" },
    },
    .symlinks = true,
    .input = "L",
    .expect = { .parent = "A", .exists = true },
  },
  {
    .name = "missing_under_symlink",
    .setup = {
      { "T", CANON_SETUP_DIR },
      { .path = "L", .kind = CANON_SETUP_SYMLINK, .target = "T" },
    },
    .symlinks = true,
    .input = "L/M",
    .expect = { .parent = "T", .leaf = "M" },
  },
  {
    .name = "empty_input",
    .expect = { .empty = true },
  },
  {
    .name = "relative_missing_bails",
    .input = "spn_canon_no_such_file_zz",
    .expect = { .empty = true },
  },
};

static sp_test_once_t symlink_once = sp_zero;

static sp_err_t symlink_probe(void* user) {
  sp_str_t dir = *(sp_str_t*)user;
  sp_mem_arena_marker_t s = sp_mem_begin_scratch();
  sp_str_t target = sp_fs_join_path(s.mem, dir, sp_str_lit("T"));
  sp_str_t link = sp_fs_join_path(s.mem, dir, sp_str_lit("L"));
  sp_fs_create_file(target);
  sp_err_t err = sp_fs_create_sym_link(target, link);
  if (!err) sp_fs_remove_file(link);
  sp_fs_remove_file(target);
  sp_mem_end_scratch(s);
  return err;
}

static bool symlinks_available(sp_test_t* t) {
  sp_str_t dir = sp_test_dir(t);
  return sp_test_once(&symlink_once, symlink_probe, &dir) == SP_OK;
}

sp_test_each(wasi_canonicalize, probe, canon_test_t, canon_tests) {
  if (it->symlinks && !symlinks_available(t)) {
    return sp_test_skip(t, "symlinks not available");
  }

  sp_mem_t mem = sp_test_arena(t);
  sp_str_t sandbox = sp_zero;
  sp_str_t input = sp_str_view(it->input);

  u32 count = 0;
  sp_carr_detect_len(it->setup, count, it->setup[count].path);
  if (count) {
    sandbox = sp_test_dir(t);
    sp_for(at, count) {
      const canon_setup_t* s = &it->setup[at];
      sp_str_t path = sp_fs_join_path(mem, sandbox, sp_str_view(s->path));
      switch (s->kind) {
        case CANON_SETUP_FILE: {
          sp_fs_create_file(path);
          break;
        }
        case CANON_SETUP_DIR: {
          sp_fs_create_dir(path);
          break;
        }
        case CANON_SETUP_SYMLINK: {
          sp_fs_create_sym_link(sp_fs_join_path(mem, sandbox, sp_str_view(s->target)), path);
          break;
        }
      }
    }
    input = sp_fs_join_path(mem, sandbox, input);
  }

  const canon_expect_t* expect = &it->expect;
#if defined(SP_WIN32)
  if (it->windows.set) {
    expect = &it->windows;
  }
#endif

  sp_str_t result = spn_dag_wasi_canonicalize(mem, input);

  if (expect->empty) {
    sp_expect_eq(t, result.len, 0u);
    return SP_OK;
  }

  sp_str_t anchor = sandbox;
  if (expect->parent && expect->parent[0]) {
    anchor = sp_fs_join_path(mem, sandbox, sp_str_view(expect->parent));
  }
  sp_str_t expected = sp_fs_canonicalize_path(mem, anchor);
  sp_expect_gt(t, expected.len, 0u);
  if (expect->leaf) {
    expected = sp_fs_join_path(mem, expected, sp_str_view(expect->leaf));
  }

  sp_expect_str_eq(t, result, expected);
  sp_expect_eq(t, sp_fs_exists(result), expect->exists);
  sp_expect_str_eq(t, spn_dag_wasi_canonicalize(mem, result), result);
  return SP_OK;
}
