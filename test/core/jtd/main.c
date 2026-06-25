#define SP_IMPLEMENTATION
#include "sp.h"

#define UTEST_IMPLEMENTATION
#include "utest.h"

#include "jtd_test.h"

UTEST_MAIN();

void jtd_expect_str(s32* utest_result, sp_str_t actual, const c8* expected) {
  EXPECT_TRUE(sp_str_equal_cstr(actual, expected));
}

void run_jtd_case(s32* utest_result, jtd_case_t c) {
  sp_mem_t mem = sp_mem_os_new();
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();

  sp_str_t path = sp_fmt(sp_mem_get_scratch(), "{}/{}", sp_fmt_cstr(JTD_TEST_JSON_DIR), sp_fmt_cstr(c.json)).value;
  sp_str_t json = sp_zero; sp_io_read_file(sp_mem_get_scratch(), path, &json);

  if (sp_str_empty(json)) {
    EXPECT_TRUE(sp_str_equal_cstr(path, "<fixture exists and is non-empty>"));
  }
  else {
    jtd_result_t result = jtd_parse(mem, json);

    if (c.error == JTD_OK) {
      EXPECT_TRUE(result.ok);
      if (result.ok && c.compare) {
        c.compare(utest_result, &result, c.expect);
      }
    }
    else {
      EXPECT_FALSE(result.ok);
      EXPECT_EQ((s32)c.error, (s32)result.diag.code);
      if (c.error_path) {
        EXPECT_TRUE(sp_str_equal_cstr(result.diag.path, c.error_path));
      }
    }

    jtd_free(&result);
  }

  sp_mem_end_scratch(scratch);
}

#include "names.c"
#include "type.c"
#include "enums.c"
#include "elements.c"
#include "properties.c"
#include "values.c"
#include "discriminator.c"
#include "ref.c"
#include "document.c"
#include "metadata.c"
#include "walk.c"
#include "conformance.c"
