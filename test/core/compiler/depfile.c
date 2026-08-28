#include "compiler.h"

#define DEPFILE_TEST_MAX_PREREQS 8

typedef struct {
  spn_err_t err;
  const c8* prereqs [DEPFILE_TEST_MAX_PREREQS];
} depfile_expect_t;

typedef struct {
  const c8* name;
  spn_cc_driver_t driver;
  depfile_expect_t expect;
} depfile_test_t;

static const depfile_test_t tests [] = {
  {
    .name = "gnu",
    .driver = SPN_CC_DRIVER_GCC,
    .expect = {
      .prereqs = { "main.c", "a.h", "b.h" },
    },
  },
  {
    .name = "gnu_no_prereqs",
    .driver = SPN_CC_DRIVER_GCC,
  },
  {
    .name = "gnu_missing_colon",
    .driver = SPN_CC_DRIVER_GCC,
    .expect = {
      .err = SPN_ERROR,
    },
  },
  {
    .name = "msvc",
    .driver = SPN_CC_DRIVER_MSVC,
    .expect = {
      .prereqs = { "a.h", "b.h" },
    },
  },
  {
    .name = "msvc_no_includes",
    .driver = SPN_CC_DRIVER_MSVC,
  },
  {
    .name = "msvc_wrong_schema",
    .driver = SPN_CC_DRIVER_MSVC,
    .expect = {
      .err = SPN_ERROR,
    },
  },
  {
    .name = "msvc_not_json",
    .driver = SPN_CC_DRIVER_MSVC,
    .expect = {
      .err = SPN_ERROR,
    },
  },
};

sp_test_each(depfile, parse, depfile_test_t, tests) {
  sp_mem_t mem = sp_test_arena(t);

  sp_str_t path = test_repo_path(mem, sp_test_format(t, "test/core/compiler/depfile/{}.d", sp_fmt_cstr(it->name)));
  sp_str_t content = sp_zero;
  sp_must(t, !sp_io_read_file(mem, path, &content));

  spn_cc_toolchain_t toolchain = test_toolchain(it->driver);
  sp_da(sp_str_t) prereqs = sp_zero;
  spn_err_t err = spn_cc_parse_depfile(mem, &toolchain, content, &prereqs);

  sp_must_eq(t, it->expect.err, err);
  if (!err) {
    sp_must_strs_eq(t, prereqs, sp_da_size(prereqs), it->expect.prereqs);
  }
  return SP_OK;
}
