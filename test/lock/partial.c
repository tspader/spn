#ifdef SPN_BUILD
#include "spn/recipes/sp.h"
#include "spn/recipes/tcc.h"

#define SPN_DEPS() \
  SPN_DEP(sp) \
  SPN_DEP(tcc)

#define SPN_LOCKS() \
  SPN_LOCK(sp, "aa17b02c")

#include "spn/gen.h"

spn_build_t spn_build() {
  return (spn_build_t) {
    .name = "partial",
    .deps = {}
  };
}
#endif

#ifndef SPN_BUILD

#define SP_IMPLEMENTATION
#include "sp.h"

s32 main(s32 n, const c8** args) {
  SP_LOG("partial test");
  return 0;
}

#endif
