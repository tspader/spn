#ifdef SPN_BUILD
#include "spn/recipes/sp.h"
#include "spn/recipes/tcc.h"
#include "spn/recipes/argparse.h"

#define SPN_DEPS() \
  SPN_DEP(sp) \
  SPN_DEP(tcc) \
  SPN_DEP(argparse)

#define SPN_LOCKS() \
  SPN_LOCK(sp, "aa17b02c58") \
  SPN_LOCK(argparse, "f71ed6c7b1") \
  SPN_LOCK(tcc, "f4e01bfcab") \

#include "spn/gen.h"

spn_build_t spn_build() {
  return (spn_build_t) {
    .name = "test_sort",
    .deps = {}
  };
}
#endif

#ifndef SPN_BUILD

#define SP_IMPLEMENTATION
#include "sp.h"

s32 main(s32 n, const c8** args) {
  SP_LOG("test sort");
  return 0;
}

#endif
