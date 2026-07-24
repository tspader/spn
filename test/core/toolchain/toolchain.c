#include "fixture.h"

typedef struct {
  const c8* program;
  const c8* program_win;
  const c8* str;
} launcher_expect_t;

typedef struct {
  fixture_launcher_t launcher;
  const c8* root;
  launcher_expect_t expect;
} launcher_test_t;

static void run_launcher_test(s32* utest_result, launcher_test_t t) {
  sp_mem_t mem = sp_mem_arena_as_allocator(ctx_get()->arena);

  spn_toolchain_launcher_t launcher = { .program = sp_str_view(t.launcher.program) };
  launcher.args = sp_da_new(mem, sp_str_t);
  sp_carr_for(t.launcher.args, it) {
    if (!t.launcher.args[it]) {
      break;
    }
    sp_da_push(launcher.args, sp_str_view(t.launcher.args[it]));
  }

  if (t.expect.program) {
    const c8* program = t.expect.program;
#if defined(SP_WIN32)
    if (t.expect.program_win) {
      program = t.expect.program_win;
    }
#endif
    spn_toolchain_launcher_t rooted = spn_toolchain_launcher_with_root(mem, launcher, sp_str_view(t.root ? t.root : ""));
    EXPECT_STR(rooted.program, program);
  }

  if (t.expect.str) {
    EXPECT_STR(spn_toolchain_launcher_to_str(mem, launcher), t.expect.str);
  }
}

UTEST(launcher, empty_root_is_identity) {
  run_launcher_test(utest_result, (launcher_test_t) {
    .launcher = { .program = "A" },
    .expect = { .program = "A" },
  });
}

UTEST(launcher, empty_program_stays_empty) {
  run_launcher_test(utest_result, (launcher_test_t) {
    .launcher = { .program = "" },
    .root = "/R",
    .expect = { .program = "" },
  });
}

UTEST(launcher, root_prefixes_program) {
  run_launcher_test(utest_result, (launcher_test_t) {
    .launcher = { .program = "B/A", .args = { "C" } },
    .root = "/R",
    .expect = { .program = "/R/B/A", .program_win = "/R/B/A.exe" },
  });
}

UTEST(launcher, to_str_program_only) {
  run_launcher_test(utest_result, (launcher_test_t) {
    .launcher = { .program = "A" },
    .expect = { .str = "A" },
  });
}

UTEST(launcher, to_str_joins_args) {
  run_launcher_test(utest_result, (launcher_test_t) {
    .launcher = { .program = "A", .args = { "B", "C" } },
    .expect = { .str = "A B C" },
  });
}

UTEST(launcher, has_cxx_requires_program) {
  spn_toolchain_info_t toolchain = sp_zero;
  EXPECT_FALSE(spn_toolchain_has_cxx(&toolchain));
  toolchain.cxx.program = sp_str_view("A");
  EXPECT_TRUE(spn_toolchain_has_cxx(&toolchain));
}
