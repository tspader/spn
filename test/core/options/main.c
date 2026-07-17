#define SP_IMPLEMENTATION
#define UTEST_IMPLEMENTATION
#include "common.h"

UTEST_STATE();

s32 options_test_entry(s32 argc, const c8** argv) {
  return utest_main(argc, argv);
}
SP_MAIN(options_test_entry)
