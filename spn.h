#include "spn/recipes/sp.h"
#include "spn/recipes/tcc.h"
#include "spn/recipes/argparse.h"

#define SPN_DEPS() \
  SPN_DEP(sp) \
  SPN_DEP(tcc) \
  SPN_DEP(argparse)

#include "spn/gen.h"

spn_build_t spn_build() {
  return (spn_build_t) {
    .name = "sp",
    .deps = {}
  };
}
