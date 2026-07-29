#include "../compiler.h"

typedef struct {
  spn_cc_exports_format_t format;
  const c8* extension;
} format_expect_t;

typedef struct {
  const c8* name;
  spn_cc_output_kind_t kind;
  spn_os_t os;
  format_expect_t expect;
} format_test_t;

static const format_test_t tests [] = {
  {
    .name = "linux_shared",
    .kind = SPN_CC_OUTPUT_SHARED_LIB,
    .os = SPN_OS_LINUX,
    .expect = { .format = SPN_CC_EXPORTS_VERSION_SCRIPT, .extension = "map" },
  },
  {
    .name = "wasi_shared",
    .kind = SPN_CC_OUTPUT_SHARED_LIB,
    .os = SPN_OS_WASI,
    .expect = { .format = SPN_CC_EXPORTS_VERSION_SCRIPT, .extension = "map" },
  },
  {
    .name = "unknown_os_shared",
    .kind = SPN_CC_OUTPUT_SHARED_LIB,
    .os = SPN_OS_NONE,
    .expect = { .format = SPN_CC_EXPORTS_VERSION_SCRIPT, .extension = "map" },
  },
  {
    .name = "macos_shared",
    .kind = SPN_CC_OUTPUT_SHARED_LIB,
    .os = SPN_OS_MACOS,
    .expect = { .format = SPN_CC_EXPORTS_SYMBOL_LIST, .extension = "exp" },
  },
  {
    .name = "windows_shared",
    .kind = SPN_CC_OUTPUT_SHARED_LIB,
    .os = SPN_OS_WINDOWS,
    .expect = { .format = SPN_CC_EXPORTS_DEF, .extension = "def" },
  },
  {
    .name = "wasi_reactor",
    .kind = SPN_CC_OUTPUT_REACTOR,
    .os = SPN_OS_WASI,
    .expect = { .format = SPN_CC_EXPORTS_WASM, .extension = "sym" },
  },
};

sp_test_each(exports_format, select, format_test_t, tests) {
  spn_cc_exports_format_t format = spn_cc_exports_format(it->kind, it->os);
  sp_expect_eq(t, format, it->expect.format);
  sp_expect_str_eq_c(t, sp_cstr_as_str(spn_cc_exports_extension(format)), it->expect.extension);
  return SP_OK;
}
