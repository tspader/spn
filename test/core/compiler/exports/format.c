#include "../common.h"

typedef struct {
  spn_cc_output_kind_t kind;
  spn_os_t os;
  spn_cc_exports_format_t format;
  const c8* extension;
} exports_format_test_t;

UTEST(exports_format, table) {
  exports_format_test_t tests [] = {
    { .kind = SPN_CC_OUTPUT_SHARED_LIB, .os = SPN_OS_LINUX,   .format = SPN_CC_EXPORTS_VERSION_SCRIPT, .extension = "map" },
    { .kind = SPN_CC_OUTPUT_SHARED_LIB, .os = SPN_OS_WASI,    .format = SPN_CC_EXPORTS_VERSION_SCRIPT, .extension = "map" },
    { .kind = SPN_CC_OUTPUT_SHARED_LIB, .os = SPN_OS_NONE,    .format = SPN_CC_EXPORTS_VERSION_SCRIPT, .extension = "map" },
    { .kind = SPN_CC_OUTPUT_SHARED_LIB, .os = SPN_OS_MACOS,   .format = SPN_CC_EXPORTS_SYMBOL_LIST,    .extension = "exp" },
    { .kind = SPN_CC_OUTPUT_SHARED_LIB, .os = SPN_OS_WINDOWS, .format = SPN_CC_EXPORTS_DEF,            .extension = "def" },
    { .kind = SPN_CC_OUTPUT_REACTOR,    .os = SPN_OS_WASI,    .format = SPN_CC_EXPORTS_WASM,           .extension = "sym" },
  };

  sp_carr_for(tests, it) {
    spn_cc_exports_format_t format = spn_cc_exports_format(tests[it].kind, tests[it].os);
    EXPECT_EQ(format, tests[it].format);
    EXPECT_TRUE(sp_str_equal_cstr(sp_cstr_as_str(spn_cc_exports_extension(format)), tests[it].extension));
  }
}
