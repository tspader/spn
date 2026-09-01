#include "../compiler.h"

typedef struct {
  const c8* name;
  spn_cc_driver_t driver;
  spn_rsp_style_t expect;
} test_t;

static const test_t tests [] = {
  { .name = "gcc", .driver = SPN_CC_DRIVER_GCC, .expect = SPN_RSP_STYLE_GNU },
  { .name = "clang", .driver = SPN_CC_DRIVER_CLANG, .expect = SPN_RSP_STYLE_WINDOWS },
  { .name = "zig", .driver = SPN_CC_DRIVER_ZIG, .expect = SPN_RSP_STYLE_WINDOWS },
  { .name = "msvc", .driver = SPN_CC_DRIVER_MSVC, .expect = SPN_RSP_STYLE_WINDOWS },
};

sp_test_each(rsp_style, select, test_t, tests) {
  sp_expect_eq(t, spn_rsp_style(it->driver), it->expect);
  return SP_OK;
}
