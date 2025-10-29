#include "spn/recipes/sp.h"
#include "spn/recipes/tcc.h"
#include "spn/recipes/argparse.h"

#define SPN_DEPS() \
  SPN_DEP(sp) \
  SPN_DEP(tcc) \
  SPN_DEP(argparse)

#define SPN_LOCKS() \
  SPN_LOCK(argparse, "HEAD") \
  SPN_LOCK(sp, "aa17b02c") \
  SPN_LOCK(tcc, "01d1b7bc") \

#include "spn/gen.h"

spn_build_t spn_build() {
  return (spn_build_t) {
    .name = "sp",
    .deps = {}
  };
}
