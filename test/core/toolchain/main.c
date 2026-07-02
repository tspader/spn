#define SP_IMPLEMENTATION
#include "sp.h"

#define UTEST_IMPLEMENTATION
#include "utest.h"

#include "test.h"

UTEST_STATE();

int main(int argc, const char *const argv[]) {
  ctx_init(ctx_get());
  s32 result = utest_main(argc, argv);
  ctx_deinit(ctx_get());
  return result;
}
