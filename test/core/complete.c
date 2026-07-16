#define SP_IMPLEMENTATION
#include "sp.h"

#include "utest.h"

#include "cli/cli.h"
#include "complete/complete.h"
#include "ctx/types.h"
#include "profile/types.h"
#include "sp/str.h"

spn_ctx_t spn;

UTEST_MAIN()

sp_cli_result_t spn_cli_init(sp_cli_t* cli) { return SP_CLI_OK; }
sp_cli_result_t spn_cli_add(sp_cli_t* cli) { return SP_CLI_OK; }
sp_cli_result_t spn_cli_update(sp_cli_t* cli) { return SP_CLI_OK; }
sp_cli_result_t spn_cli_clean(sp_cli_t* cli) { return SP_CLI_OK; }
sp_cli_result_t spn_cli_build(sp_cli_t* cli) { return SP_CLI_OK; }
sp_cli_result_t spn_cli_run(sp_cli_t* cli) { return SP_CLI_OK; }
sp_cli_result_t spn_cli_test(sp_cli_t* cli) { return SP_CLI_OK; }
sp_cli_result_t spn_cli_generate(sp_cli_t* cli) { return SP_CLI_OK; }
sp_cli_result_t spn_cli_which(sp_cli_t* cli) { return SP_CLI_OK; }
sp_cli_result_t spn_cli_graph(sp_cli_t* cli) { return SP_CLI_OK; }
sp_cli_result_t spn_cli_publish(sp_cli_t* cli) { return SP_CLI_OK; }
sp_cli_result_t spn_cli_index(sp_cli_t* cli) { return SP_CLI_OK; }
sp_cli_result_t spn_cli_index_list(sp_cli_t* cli) { return SP_CLI_OK; }
sp_cli_result_t spn_cli_index_path(sp_cli_t* cli) { return SP_CLI_OK; }
sp_cli_result_t spn_cli_index_sync(sp_cli_t* cli) { return SP_CLI_OK; }
sp_cli_result_t spn_cli_completions(sp_cli_t* cli) { return SP_CLI_OK; }

#define COMPLETE_TEST_MAX_WORDS 8
#define COMPLETE_TEST_MAX_CANDIDATES 16
#define COMPLETE_TEST_MAX_EXCLUDES 8

typedef struct {
  const c8* candidates [COMPLETE_TEST_MAX_CANDIDATES];
  const c8* excludes [COMPLETE_TEST_MAX_EXCLUDES];
} complete_expect_t;

typedef struct {
  const c8* words [COMPLETE_TEST_MAX_WORDS];
  bool pkg;
  complete_expect_t expect;
} complete_test_t;

static spn_pkg_info_t make_pkg() {
  spn_pkg_info_t pkg = sp_zero;

  spn_target_info_t bin = { .name = sp_str_lit("main"), .kind = SPN_TARGET_EXE };
  sp_str_om_insert(pkg.exes, bin.name, bin);

  spn_target_info_t lib = { .name = sp_str_lit("A"), .kind = SPN_TARGET_LIB };
  sp_str_om_insert(pkg.libs, lib.name, lib);

  spn_target_info_t test = { .name = sp_str_lit("B"), .kind = SPN_TARGET_TEST };
  sp_str_om_insert(pkg.tests, test.name, test);

  spn_target_info_t script = { .name = sp_str_lit("C"), .kind = SPN_TARGET_SCRIPT };
  sp_str_om_insert(pkg.scripts, script.name, script);

  spn_profile_info_t profile = { .name = sp_str_lit("D") };
  sp_str_om_insert(pkg.profiles, profile.name, profile);

  spn_toolchain_info_t toolchain = { .name = sp_str_lit("E") };
  sp_str_om_insert(pkg.toolchains, toolchain.name, toolchain);

  return pkg;
}

static bool has_line(sp_str_t output, const c8* needle) {
  sp_str_for_line(output, it) {
    if (sp_str_equal_cstr(it.line, needle)) {
      return true;
    }
  }
  return false;
}

static void run_complete_test(s32* utest_result, complete_test_t t) {
  c8 buffer [4096];
  sp_io_mem_writer_t out = sp_zero;
  sp_io_mem_writer_from_buffer(&out, buffer, sizeof(buffer));

  spn_pkg_info_t pkg = sp_zero;
  if (t.pkg) {
    pkg = make_pkg();
  }

  u32 num_words = 0;
  sp_carr_for(t.words, it) {
    if (!t.words[it]) {
      break;
    }
    num_words++;
  }

  spn_complete((spn_complete_desc_t) {
    .root = spn_cli(),
    .pkg = t.pkg ? &pkg : SP_NULLPTR,
    .words = t.words,
    .num_words = num_words,
    .io = &out.base,
  });

  sp_str_t output = sp_io_mem_writer_as_str(&out);
  utest_kv("output", output);

  sp_carr_for(t.expect.candidates, it) {
    if (!t.expect.candidates[it]) {
      break;
    }
    utest_kv("candidate", sp_cstr_as_str(t.expect.candidates[it]));
    EXPECT_TRUE(has_line(output, t.expect.candidates[it]));
  }

  sp_carr_for(t.expect.excludes, it) {
    if (!t.expect.excludes[it]) {
      break;
    }
    utest_kv("exclude", sp_cstr_as_str(t.expect.excludes[it]));
    EXPECT_FALSE(has_line(output, t.expect.excludes[it]));
  }
}

UTEST(complete, commands) {
  complete_test_t tests [] = {
    { .words = { "spn", "" },                .expect = { .candidates = { "init", "add", "update", "clean", "build", "run", "test", "publish", "index", "completions" }, .excludes = { "list", "which", "graph", "generate" } } },
    { .words = { "spn", "bu" },              .expect = { .candidates = { "build" }, .excludes = { "run", "init" } } },
    { .words = { "spn", "index", "" },       .expect = { .candidates = { "list", "path", "sync" }, .excludes = { "build" } } },
    { .words = { "spn", "completions", "" }, .expect = { .candidates = { "bash", "zsh", "fish", "powershell" } } },
  };

  sp_carr_for(tests, it) {
    run_complete_test(utest_result, tests[it]);
  }
}

UTEST(complete, flags) {
  complete_test_t tests [] = {
    { .words = { "spn", "build", "-" },    .expect = { .candidates = { "--profile", "--test", "--force", "--verbose", "--help" }, .excludes = { "--bare" } } },
    { .words = { "spn", "build", "--te" }, .expect = { .candidates = { "--test" }, .excludes = { "--target", "--toolchain" } } },
    { .words = { "spn", "-" },             .expect = { .candidates = { "--project-dir", "--verbose", "--help" }, .excludes = { "--profile" } } },
  };

  sp_carr_for(tests, it) {
    run_complete_test(utest_result, tests[it]);
  }
}

UTEST(complete, values) {
  complete_test_t tests [] = {
    { .words = { "spn", "build", "-p", "" },          .pkg = true, .expect = { .candidates = { "default", "debug", "release", "D" }, .excludes = { "main" } } },
    { .words = { "spn", "build", "--profile", "de" }, .pkg = true, .expect = { .candidates = { "default", "debug" }, .excludes = { "release", "D" } } },
    { .words = { "spn", "build", "-p", "" },                       .expect = { .candidates = { "default", "debug", "release" }, .excludes = { "D" } } },
    { .words = { "spn", "build", "-m", "" },                       .expect = { .candidates = { "debug", "release" }, .excludes = { "default" } } },
    { .words = { "spn", "build", "--toolchain", "" }, .pkg = true, .expect = { .candidates = { "zig", "E" } } },
    { .words = { "spn", "build", "-fp", "" },         .pkg = true, .expect = { .candidates = { "default", "D" } } },
  };

  sp_carr_for(tests, it) {
    run_complete_test(utest_result, tests[it]);
  }
}

UTEST(complete, targets) {
  complete_test_t tests [] = {
    { .words = { "spn", "build", "" },                   .pkg = true, .expect = { .candidates = { "main", "A", "B", "C" } } },
    { .words = { "spn", "build", "" },                                .expect = { .excludes = { "main", "A", "B", "C" } } },
    { .words = { "spn", "build", "--test", "" },         .pkg = true, .expect = { .candidates = { "B" }, .excludes = { "main", "A", "C" } } },
    { .words = { "spn", "build", "--bin", "--lib", "" }, .pkg = true, .expect = { .candidates = { "main", "A" }, .excludes = { "B", "C" } } },
    { .words = { "spn", "build", "main", "" },           .pkg = true, .expect = { .candidates = { "A" }, .excludes = { "main" } } },
    { .words = { "spn", "build", "--opt=2", "" },        .pkg = true, .expect = { .candidates = { "main" }, .excludes = { "2" } } },
    { .words = { "spn", "test", "" },                    .pkg = true, .expect = { .candidates = { "B" }, .excludes = { "main", "A", "C" } } },
    { .words = { "spn", "test", "B", "" },               .pkg = true, .expect = { .excludes = { "B", "main" } } },
    { .words = { "spn", "run", "" },                     .pkg = true, .expect = { .candidates = { "C" }, .excludes = { "main", "B" } } },
  };

  sp_carr_for(tests, it) {
    run_complete_test(utest_result, tests[it]);
  }
}

UTEST(complete, raw) {
  complete_test_t tests [] = {
    { .words = { "spn", "build", "--", "-" }, .pkg = true, .expect = { .excludes = { "--profile", "--help" } } },
    { .words = { "spn", "build", "--", "" },  .pkg = true, .expect = { .candidates = { "main" } } },
  };

  sp_carr_for(tests, it) {
    run_complete_test(utest_result, tests[it]);
  }
}
