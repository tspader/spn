#include "spn_test.h"
#include "render.h"

#define SHA_A "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
#define SHA_B "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
#define SHA_C "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"

typedef struct {
  const c8* name;
  const c8* shasums;
  const c8* golden;
  installer_err_t err;
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
    .golden = "unix_only",
  },
  {
    .name = "no_asset",
    .shasums = r(SHA_B),
    .err = INSTALLER_ERR_MALFORMED,
  },
  {
    .name = "short_sha",
    .shasums = r("abc123  spn-x86_64-linux.tar.gz"),
    .err = INSTALLER_ERR_MALFORMED,
  },
  {
    .name = "upper_sha",
    .shasums = r("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA  spn-x86_64-linux.tar.gz"),
    .err = INSTALLER_ERR_MALFORMED,
  },
  {
    .name = "bad_prefix",
    .shasums = r(SHA_B "  zig-x86_64-linux.tar.gz"),
    .err = INSTALLER_ERR_ASSET,
  },
  {
    .name = "bad_ext",
    .shasums = r(SHA_B "  spn-x86_64-linux.tar.xz"),
    .err = INSTALLER_ERR_ASSET,
  },
  {
    .name = "windows_tar",
    .shasums = r(SHA_C "  spn-x86_64-windows.tar.gz"),
    .err = INSTALLER_ERR_ASSET,
  },
  {
    .name = "unix_zip",
    .shasums = r(SHA_B "  spn-x86_64-linux.zip"),
    .err = INSTALLER_ERR_ASSET,
  },
  {
    .name = "duplicate",
    .shasums =
      r(SHA_B "  spn-x86_64-linux.tar.gz")
      r(SHA_A "  spn-x86_64-linux.tar.gz"),
    .err = INSTALLER_ERR_DUPLICATE,
  },
  {
    .name = "empty",
    .shasums = "\n\n",
    .err = INSTALLER_ERR_EMPTY,
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
  sp_must_eq(t, it->err, result.err);
  if (result.err) {
    sp_expect(t, !sp_str_empty(result.message));
    return SP_OK;
  }

  const c8* golden = it->golden ? it->golden : it->name;
  sp_expect_golden(t, sp_test_format(t, "golden/{}.sh", sp_fmt_cstr(golden)),
    test_read_file(mem, sp_fs_join_path(mem, sp_test_dir(t), sp_str_lit("install.sh"))));
  sp_expect_golden(t, sp_test_format(t, "golden/{}.ps1", sp_fmt_cstr(golden)),
    test_read_file(mem, sp_fs_join_path(mem, sp_test_dir(t), sp_str_lit("install.ps1"))));
  return SP_OK;
}
