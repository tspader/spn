#include "spn/spn.h"

#define SPN_PACKAGE sp
#define SPN_OPTIONS() \
  SPN_OPTION(spn_sp_backend_t, backend) \
  SPN_OPTION(int, bar) \
  SPN_OPTION(float, baz)

typedef enum {
  SP_BACKEND_NATIVE,
  SP_BACKEND_SDL,
} spn_sp_backend_t;

#include "spn/gen.h"

void sp_package(spn_dep_builder_t build) {
  spn_copy(&build, SPN_DIR_SOURCE, "sp.h", SPN_DIR_INCLUDE, NULL);
}

spn_recipe_info_t sp() {
  return (spn_recipe_info_t) {
    .name = "sp",
    .git = "tspader/sp",
    .kinds = {
      SPN_DEP_BUILD_KIND_SOURCE
    },
    .package = sp_package,
  };
}

