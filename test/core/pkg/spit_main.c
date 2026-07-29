// The sp and spit implementations live alone in this TU, like main.c does
// for the utest binaries
#define SP_IMPLEMENTATION
#include "sp.h"

#include "spit.h"

s32 main(s32 argc, const c8** argv) {
  return sp_test_main(argc, argv, SP_NULLPTR);
}
