#define SP_IMPLEMENTATION
#include "sp.h"
#include "sp/macro.h"

#include "utest.h"

#include "closure.h"
#include "ctx/types.h"

spn_ctx_t spn;

UTEST_MAIN()

void expect_target_names(s32* utest_result, sp_da(spn_target_unit_t*) targets, const c8* const* expect, u32 max) {
  u32 expected = 0;
  sp_for(it, max) {
    if (!expect[it]) {
      break;
    }
    expected++;
  }

  ASSERT_EQ(expected, sp_da_size(targets));
  sp_for(it, expected) {
    EXPECT_TRUE(sp_str_equal(targets[it]->info->name, sp_str_view(expect[it])));
  }
}
