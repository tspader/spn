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
  sp_mem_arena_t* arena = sp_mem_arena_new(spn_allocator);
  sp_context_push_arena(arena);

  sp_str_t path = sp_format("{}/{}", SP_FMT_CSTR(JTD_TEST_JSON_DIR), SP_FMT_CSTR(c.json));
  sp_str_t json = sp_zero; sp_io_read_file(spn_allocator, path, &json);

  if (sp_str_empty(json)) {
    EXPECT_TRUE(sp_str_equal_cstr(path, "<fixture exists and is non-empty>"));
  }
  else {
    jtd_root_t       root = SP_ZERO_INITIALIZE();
    jtd_diagnostic_t diag = SP_ZERO_INITIALIZE();
    bool ok = jtd_parse(json, &root, &diag);

    if (c.error == JTD_OK) {
      EXPECT_TRUE(ok);
      if (ok && c.compare) {
        c.compare(utest_result, &root, c.expect);
      }
    }
    else {
      EXPECT_FALSE(ok);
      EXPECT_EQ((s32)c.error, (s32)diag.code);
      if (c.error_path) {
        EXPECT_TRUE(sp_str_equal_cstr(diag.path, c.error_path));
      }
    }
  }

  sp_context_pop();
  sp_mem_arena_destroy(arena);
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
#include "walk.c"
#include "conformance.c"
