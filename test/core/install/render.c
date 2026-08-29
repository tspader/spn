#include "spn_test.h"
#include "render.h"

#define SHA_A "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
#define SHA_B "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
#define SHA_C "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"

typedef struct {
  installer_err_t err;
  const c8* message;
  const c8* golden;
} expect_t;

typedef struct {
  const c8* name;
  const c8* shasums;
  expect_t expect;
} render_test_t;

static const render_test_t tests [] = {
  {
    .name = "full",
    .shasums =
      r(SHA_B "  spn-x86_64-linux.tar.gz")
      r(SHA_A "  spn-aarch64-macos.tar.gz")
      r(SHA_C "  spn-x86_64-windows.zip"),
  },
  {
    .name = "unix_only",
    .shasums = r(SHA_B "  spn-x86_64-linux.tar.gz"),
  },
  {
    .name = "binary_marker",
    .shasums = r(SHA_B " *spn-x86_64-linux.tar.gz"),
    .expect = { .golden = "unix_only" },
  },
  {
    .name = "no_asset",
    .shasums = r(SHA_B),
    .expect = {
      .err = INSTALLER_ERR_FIELDS,
      .message = "line 1: expected a sha and an asset, got " SHA_B,
    },
  },
  {
    .name = "short_sha",
    .shasums = r("abc123  spn-x86_64-linux.tar.gz"),
    .expect = {
      .err = INSTALLER_ERR_SHA,
      .message = "line 1: abc123 is not a lowercase sha256",
    },
  },
  {
    .name = "upper_sha",
    .shasums = r("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA  spn-x86_64-linux.tar.gz"),
    .expect = {
      .err = INSTALLER_ERR_SHA,
      .message = "line 1: AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA is not a lowercase sha256",
    },
  },
  {
    .name = "bad_prefix",
    .shasums = r(SHA_B "  zig-x86_64-linux.tar.gz"),
    .expect = {
      .err = INSTALLER_ERR_ASSET,
      .message = "line 1: zig-x86_64-linux.tar.gz is not a spn release asset",
    },
  },
  {
    .name = "bad_ext",
    .shasums = r(SHA_B "  spn-x86_64-linux.tar.xz"),
    .expect = {
      .err = INSTALLER_ERR_ASSET,
      .message = "line 1: spn-x86_64-linux.tar.xz is not a spn release asset",
    },
  },
  {
    .name = "windows_tar",
    .shasums = r(SHA_C "  spn-x86_64-windows.tar.gz"),
    .expect = {
      .err = INSTALLER_ERR_PAIRING,
      .message = "line 1: spn-x86_64-windows.tar.gz pairs a platform with the wrong archive format",
    },
  },
  {
    .name = "unix_zip",
    .shasums = r(SHA_B "  spn-x86_64-linux.zip"),
    .expect = {
      .err = INSTALLER_ERR_PAIRING,
      .message = "line 1: spn-x86_64-linux.zip pairs a platform with the wrong archive format",
    },
  },
  {
    .name = "duplicate",
    .shasums =
      r(SHA_B "  spn-x86_64-linux.tar.gz")
      r(SHA_A "  spn-x86_64-linux.tar.gz"),
    .expect = {
      .err = INSTALLER_ERR_DUPLICATE,
      .message = "line 2: duplicate target x86_64-linux",
    },
  },
  {
    .name = "empty",
    .shasums = "\n\n",
    .expect = {
      .err = INSTALLER_ERR_EMPTY,
      .message = "no assets",
    },
  },
};

sp_test_each(installer, render, render_test_t, tests) {
  sp_mem_t mem = sp_test_arena(t);

  installer_result_t result = installer_render(mem, (installer_config_t) {
    .shasums = sp_cstr_as_str(it->shasums),
    .templates = test_repo_path(mem, sp_str_lit("tools/install/templates")),
    .out = sp_test_dir(t),
    .version = sp_str_lit("0.0.0"),
    .tag = sp_str_lit("v0.0.0"),
    .repo = sp_str_lit("A/B"),
  });
  sp_must_eq(t, it->expect.err, result.err);
  if (result.err) {
    sp_expect_str_eq_c(t, installer_result_to_str(mem, result), it->expect.message);
    return SP_OK;
  }

  const c8* golden = it->expect.golden ? it->expect.golden : it->name;
  sp_expect_golden(t, sp_test_format(t, "golden/{}.sh", sp_fmt_cstr(golden)),
    test_read_file(mem, sp_fs_join_path(mem, sp_test_dir(t), sp_str_lit("install.sh"))));
  sp_expect_golden(t, sp_test_format(t, "golden/{}.ps1", sp_fmt_cstr(golden)),
    test_read_file(mem, sp_fs_join_path(mem, sp_test_dir(t), sp_str_lit("install.ps1"))));
  return SP_OK;
}
