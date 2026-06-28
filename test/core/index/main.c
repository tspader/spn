#define SP_IMPLEMENTATION
#include "sp.h"

#define UTEST_IMPLEMENTATION
#include "utest.h"

#include "test.h"

UTEST_STATE();
s32 main(s32 num_args, const c8** args) {
  ctx_t* ctx = ctx_get();
  ctx_init(ctx_get());

  s32 result = utest_main(num_args, args);

  ctx_deinit(ctx_get());
  return result;
}
