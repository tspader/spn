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
    .name = "empty_root_is_identity",
    .launcher = { .program = "A" },
    .expect = { .program = "A" },
  },
  {
    .name = "empty_program_stays_empty",
    .launcher = { .program = "" },
    .root = "/R",
    .expect = { .program = "" },
  },
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
    spn_path_t root = { .sub = sp_str_view(it->root ? it->root : "") };
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
