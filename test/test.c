#define SPN_BUILD

#ifdef SPN_BUILD
#include "spn/recipes/sp.h"
#include "spn/recipes/tcc.h"
#include "spn/recipes/argparse.h"

#define SPN_DEPS() \
  SPN_DEP(sp) \
  SPN_DEP(tcc) \
  SPN_DEP(argparse)

#define SPN_LOCKS() \
  SPN_LOCK(sp,  "aa17b02c") \
  SPN_LOCK(tcc, "01d1b7bc")

#include "spn/gen.h"

spn_build_t spn_build() {
  return (spn_build_t) {
    .name = "sp",
    .deps = {
      .sp = {},
      .tcc = {}
    }
  };
}
#endif

#ifndef SPN_BUILD

#define SP_IMPLEMENTATION
#include "sp.h"

s32 main(s32 n, const c8** args) {
  sp_str_t str = SP_LIT("hello");
  SP_LOG("hello, {:fg brightcyan}", SP_FMT_STR(str));
  return 0;
}

#endif
