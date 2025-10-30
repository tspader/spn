#define SPN_VERSION() 100
#define SPN_COMMIT() "85d0ea3af1a0"

#define SPN_DEPS() \
  SPN_DEP(sp) \
  SPN_DEP(tcc) \
  SPN_DEP(argparse)

#define SPN_LOCKS()

#include "spn/gen/project.h"

#ifdef SPN_BUILD
#include "spn/recipes/sp.h"
#include "spn/recipes/tcc.h"
#include "spn/recipes/argparse.h"

#include "spn/gen.h"

spn_build_t spn_build() {
  return (spn_build_t) {
    .name = "sp",
    .deps = {}
  };
}
#endif
