#include "spn_test.h"

#include "codegen/codegen.h"
#include "codegen/lower.h"
#include "config.gen.h"
#include "intern/intern.h"
#include "toml/loader.h"
#include "arg.h"
#include "toolchain/types.h"

#define CONFIG_MAX_TOOLCHAINS 2
#define CONFIG_MAX_ISSUES 2

typedef struct {
  const c8* name;
  spn_cc_driver_t driver;
  test_arg_t compiler;
  test_arg_t archiver;
} toolchain_t;

typedef struct {
  spn_err_t code;
  const c8* path;
} issue_t;

typedef struct {
  toolchain_t toolchains [CONFIG_MAX_TOOLCHAINS];
  issue_t issues [CONFIG_MAX_ISSUES];
} expect_t;

typedef struct {
  const c8* name;
  const c8* config;
  expect_t expect;
} test_t;

static const test_t tests [] = {
  {
    .name = "toolchain",
    .config = "toolchain",
    .expect = {
      .toolchains = {
        { .name = "T", .driver = SPN_CC_DRIVER_GCC, .compiler = { .name = "C" }, .archiver = { .name = "A" } },
      },
    },
  },
  {
    .name = "toolchain_incomplete",
    .config = "toolchain_incomplete",
    .expect = {
      .toolchains = {
        { .name = "T", .driver = SPN_CC_DRIVER_GCC, .compiler = { .name = "C" }, .archiver = { .name = "A" } },
        { .name = "U", .driver = SPN_CC_DRIVER_GCC, .archiver = { .name = "A" } },
      },
      .issues = {
        { SPN_ERR_CODEGEN_MISSING_KEY, "toolchain[1].compiler" },
      },
    },
  },
  {
    .name = "toolchain_program_absolute",
    .config = "toolchain_program_absolute",
    .expect = {
      .toolchains = {
        { .name = "T", .driver = SPN_CC_DRIVER_GCC, .compiler = { .path = "/T/C" }, .archiver = { .name = "A" } },
      },
    },
  },
  {
    .name = "toolchain_duplicate_name",
    .config = "toolchain_duplicate",
    .expect = {
      .toolchains = {
        { .name = "T", .driver = SPN_CC_DRIVER_GCC, .compiler = { .name = "C" }, .archiver = { .name = "A" } },
        { .name = "T", .driver = SPN_CC_DRIVER_GCC, .compiler = { .name = "D" }, .archiver = { .name = "A" } },
      },
      .issues = {
        { SPN_ERR_CODEGEN_DUPLICATE_KEY, "toolchain[1].name" },
      },
    },
  },
  {
    .name = "toolchain_program_relative",
    .config = "toolchain_program_relative",
    .expect = {
      .toolchains = {
        { .name = "T", .driver = SPN_CC_DRIVER_GCC, .archiver = { .name = "A" } },
      },
      .issues = {
        { SPN_ERR_CODEGEN_UNROOTED, "toolchain[0].compiler" },
      },
    },
  },
};

sp_test_each(config, lower, test_t, tests) {
  sp_mem_t mem = sp_test_arena(t);
  spn_toml_loader_t ctx = sp_zero;
  spn_toml_loader_init(&ctx, mem, sp_intern_new(mem));

  sp_str_t file = sp_fmt(mem, "{}.toml", sp_fmt_cstr(it->config)).value;
  sp_str_t path = sp_fs_join_path(mem, test_repo_path(mem, sp_str_lit(CONFIG_DIR)), file);
  spn_cg_config_t cg = sp_zero;
  spn_codegen_load_config(&ctx, path, &cg);

  sp_da(spn_toolchain_decl_t) toolchains = spn_toolchains_lower_list(&ctx, SPN_PATH_ROOT_NONE, cg.toolchain);

  const expect_t* expect = &it->expect;
  u32 num_issues = 0;
  sp_carr_detect_len(expect->issues, num_issues, expect->issues[num_issues].code);
  sp_must_eq(t, num_issues, (u32)sp_da_size(ctx.issues));
  sp_for(n, num_issues) {
    sp_expect_eq(t, (u32)expect->issues[n].code, (u32)ctx.issues[n].code);
    sp_expect_str_eq_c(t, ctx.issues[n].path, expect->issues[n].path);
  }

  u32 num_toolchains = 0;
  sp_carr_detect_len(expect->toolchains, num_toolchains, expect->toolchains[num_toolchains].name);
  sp_must_eq(t, num_toolchains, (u32)sp_da_size(toolchains));
  sp_for(n, num_toolchains) {
    const toolchain_t* expected = &expect->toolchains[n];
    sp_expect_str_eq_c(t, toolchains[n].name, expected->name);
    sp_expect_eq(t, (u32)expected->driver, (u32)toolchains[n].driver);
    if (test_check_arg(t, toolchains[n].compiler.program, expected->compiler)) return SP_ERR;
    if (test_check_arg(t, toolchains[n].archiver.program, expected->archiver)) return SP_ERR;
  }
  return SP_OK;
}
