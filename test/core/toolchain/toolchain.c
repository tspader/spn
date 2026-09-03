#include "toolchain.h"

typedef struct {
  const c8* program;
  const c8* program_win;
  const c8* str;
} launcher_expect_t;

typedef struct {
  const c8* name;
  fixture_launcher_t launcher;
  const c8* root;
  launcher_expect_t expect;
} launcher_test_t;

static const launcher_test_t tests [] = {
  {
    .name = "root_prefixes_program",
    .launcher = { .program = "B/A", .args = { "C" } },
    .root = "/R",
    .expect = { .program = "/R/B/A", .program_win = "/R/B/A.exe" },
  },
  {
    .name = "to_str_program_only",
    .launcher = { .program = "A" },
    .expect = { .str = "A" },
  },
  {
    .name = "to_str_joins_args",
    .launcher = { .program = "A", .args = { "B", "C" } },
    .expect = { .str = "A B C" },
  },
};

typedef struct {
  spn_toolchain_ref_kind_t kind;
  const c8* name;
} ref_expect_t;

typedef struct {
  const c8* name;
  const c8* str;
  ref_expect_t expect;
} ref_test_t;

static const ref_test_t ref_tests [] = {
  { .name = "empty_is_unset", .str = "", .expect = { .name = "" } },
  { .name = "auto_is_automatic", .str = "auto", .expect = { .kind = SPN_TOOLCHAIN_REF_AUTO, .name = "" } },
  { .name = "anything_else_is_named", .str = "A", .expect = { .kind = SPN_TOOLCHAIN_REF_NAMED, .name = "A" } },
};

sp_test_each(launcher, ref, ref_test_t, ref_tests) {
  spn_toolchain_ref_t ref = spn_toolchain_ref_from_str(sp_cstr_as_str(it->str));
  sp_expect_eq(t, (u32)it->expect.kind, (u32)ref.kind);
  sp_expect_str_eq_c(t, ref.name, it->expect.name);
  return SP_OK;
}

sp_test_each(launcher, resolve, launcher_test_t, tests) {
  sp_mem_t mem = sp_test_arena(t);
  spn_path_roots_t roots = sp_zero;

  spn_toolchain_launcher_t launcher = { .program = spn_arg_lit(sp_str_view(it->launcher.program)) };
  launcher.args = sp_da_new(mem, sp_str_t);
  sp_carr_for(it->launcher.args, at) {
    if (!it->launcher.args[at]) {
      break;
    }
    sp_da_push(launcher.args, sp_str_view(it->launcher.args[at]));
  }

  if (it->expect.program) {
    const c8* program = it->expect.program;
#if defined(SP_WIN32)
    if (it->expect.program_win) {
      program = it->expect.program_win;
    }
#endif
    spn_path_t root = { .sub = sp_cstr_as_str(it->root) };
    spn_toolchain_launcher_t rooted = spn_toolchain_launcher_with_root(mem, launcher, root);
    sp_expect_str_eq_c(t, spn_arg_str(&roots, mem, rooted.program), program);
  }

  if (it->expect.str) {
    sp_expect_str_eq_c(t, spn_toolchain_launcher_to_str(&roots, mem, launcher), it->expect.str);
  }

  return SP_OK;
}

sp_test(launcher, has_cxx_requires_program) {
  spn_toolchain_info_t toolchain = sp_zero;
  sp_expect(t, !spn_toolchain_has_cxx(&toolchain));
  toolchain.cxx.program = spn_arg_lit(sp_str_view("A"));
  sp_expect(t, spn_toolchain_has_cxx(&toolchain));
  return SP_OK;
}
