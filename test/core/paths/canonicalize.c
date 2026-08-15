#include "paths/paths_test.h"

#define CANONICALIZE_MAX_SETUP 8

typedef enum {
  CANONICALIZE_SETUP_FILE,
  CANONICALIZE_SETUP_DIR,
  CANONICALIZE_SETUP_SYMLINK,
} canonicalize_setup_kind_t;

typedef struct {
  const c8* path;
  canonicalize_setup_kind_t kind;
  const c8* target;
} canonicalize_setup_t;

typedef struct {
  spn_path_root_t root;
  const c8* sub;
} canonicalize_ref_t;

typedef struct {
  const c8* name;
  canonicalize_setup_t setup [CANONICALIZE_MAX_SETUP];
  bool symlinks;
  paths_test_roots_t roots;
  canonicalize_ref_t input;
  canonicalize_ref_t expect;
} canonicalize_test_t;

static const canonicalize_test_t canonicalize_tests [] = {
  {
    .name = "direct_spelling_is_identity",
    .setup = {
      { "A", CANONICALIZE_SETUP_DIR },
      { "A/F" },
    },
    .roots = { .project = "." },
    .input = { SPN_PATH_ROOT_PROJECT, "A/F" },
    .expect = { SPN_PATH_ROOT_PROJECT, "A/F" },
  },
  {
    .name = "alias_collapses_to_direct_spelling",
    .setup = {
      { "A", CANONICALIZE_SETUP_DIR },
      { "A/F" },
      { .path = "L", .kind = CANONICALIZE_SETUP_SYMLINK, .target = "A" },
    },
    .symlinks = true,
    .roots = { .project = "." },
    .input = { SPN_PATH_ROOT_PROJECT, "L/F" },
    .expect = { SPN_PATH_ROOT_PROJECT, "A/F" },
  },
  {
    .name = "missing_passes_through",
    .roots = { .project = "." },
    .input = { SPN_PATH_ROOT_PROJECT, "M" },
    .expect = { SPN_PATH_ROOT_PROJECT, "M" },
  },
  {
    .name = "alias_outside_roots_collapses",
    .setup = {
      { "A", CANONICALIZE_SETUP_DIR },
      { .path = "L", .kind = CANONICALIZE_SETUP_SYMLINK, .target = "A" },
    },
    .symlinks = true,
    .input = { SPN_PATH_ROOT_NONE, "L" },
    .expect = { SPN_PATH_ROOT_NONE, "A" },
  },
  {
    .name = "missing_outside_roots_passes_through",
    .input = { SPN_PATH_ROOT_NONE, "M" },
    .expect = { SPN_PATH_ROOT_NONE, "M" },
  },
  {
    .name = "existing_classifies_into_a_nested_root",
    .setup = {
      { "V", CANONICALIZE_SETUP_DIR },
      { "V/T", CANONICALIZE_SETUP_DIR },
      { "V/T/I", CANONICALIZE_SETUP_DIR },
      { "V/T/I/X" },
    },
    .roots = { .project = ".", .toolchain = "V/T" },
    .input = { SPN_PATH_ROOT_PROJECT, "V/T/I/X" },
    .expect = { SPN_PATH_ROOT_TOOLCHAIN, "I/X" },
  },
  {
    .name = "missing_classifies_into_a_nested_root",
    .setup = {
      { "V", CANONICALIZE_SETUP_DIR },
      { "V/T", CANONICALIZE_SETUP_DIR },
    },
    .roots = { .project = ".", .toolchain = "V/T" },
    .input = { SPN_PATH_ROOT_PROJECT, "V/T/M" },
    .expect = { SPN_PATH_ROOT_TOOLCHAIN, "M" },
  },
  {
    .name = "landing_on_a_nested_root",
    .setup = {
      { "V", CANONICALIZE_SETUP_DIR },
      { "V/T", CANONICALIZE_SETUP_DIR },
    },
    .roots = { .project = ".", .toolchain = "V/T" },
    .input = { SPN_PATH_ROOT_PROJECT, "V/T" },
    .expect = { .root = SPN_PATH_ROOT_TOOLCHAIN },
  },
};

static spn_path_roots_t canonicalize_roots(sp_mem_t mem, sp_str_t sandbox, paths_test_roots_t spec) {
  spn_path_roots_t rel = sp_zero;
  paths_test_roots_build(spec, &rel);

  spn_path_roots_t roots = sp_zero;
  sp_for(it, SPN_PATH_ROOT_COUNT) {
    if (sp_str_empty(rel.dirs[it])) {
      continue;
    }
    roots.dirs[it] = sp_str_equal(rel.dirs[it], sp_str_lit(".")) ? sandbox : sp_fs_join_path(mem, sandbox, rel.dirs[it]);
  }
  return roots;
}

static spn_path_t canonicalize_ref(sp_mem_t mem, sp_str_t sandbox, canonicalize_ref_t ref) {
  sp_str_t sub = ref.sub ? sp_str_view(ref.sub) : sp_str_lit("");
  if (ref.root == SPN_PATH_ROOT_NONE) {
    return (spn_path_t) { .sub = sp_fs_join_path(mem, sandbox, sub) };
  }
  return (spn_path_t) { .root = ref.root, .sub = sub };
}

sp_test_each(paths_canonicalize, resolve, canonicalize_test_t, canonicalize_tests) {
  sp_mem_t mem = sp_test_arena(t);
  sp_str_t sandbox = sp_fs_canonicalize_path(mem, sp_test_dir(t));
  sp_expect_gt(t, sandbox.len, 0u);

  u32 count = 0;
  sp_carr_detect_len(it->setup, count, it->setup[count].path);
  sp_for(at, count) {
    const canonicalize_setup_t* s = &it->setup[at];
    sp_str_t path = sp_fs_join_path(mem, sandbox, sp_str_view(s->path));
    switch (s->kind) {
      case CANONICALIZE_SETUP_FILE: {
        sp_fs_create_file(path);
        break;
      }
      case CANONICALIZE_SETUP_DIR: {
        sp_fs_create_dir(path);
        break;
      }
      case CANONICALIZE_SETUP_SYMLINK: {
        if (sp_fs_create_sym_link(sp_fs_join_path(mem, sandbox, sp_str_view(s->target)), path)) {
          return sp_test_skip(t, "symlinks not available");
        }
        break;
      }
    }
  }

  spn_path_roots_t roots = canonicalize_roots(mem, sandbox, it->roots);
  spn_path_t input = canonicalize_ref(mem, sandbox, it->input);
  spn_path_t expect = canonicalize_ref(mem, sandbox, it->expect);

  spn_path_t canonical = spn_path_canonicalize(mem, &roots, input);
  sp_expect_eq(t, canonical.root, expect.root);
  sp_expect_str_eq(t, canonical.sub, expect.sub);
  return SP_OK;
}
