#include "spn/spn.h"

#define SPN_PACKAGE tcc
#define SPN_OPTIONS()

#include "spn/gen/recipe.h"

void tcc_build(spn_dep_builder_t build) {
  spn_autoconf(&build, (spn_autoconf_t){});
  spn_make(&build, (spn_make_t) {});
}

void tcc_package(spn_dep_builder_t build) {
  spn_make(&build, (spn_make_t) {
    .target = "install"
  });
}

spn_recipe_info_t tcc() {
  return (spn_recipe_info_t) {
    .name = "tcc",
    .git = "TinyCC/tinycc",
    .kinds = {
      SPN_DEP_BUILD_KIND_STATIC,
      SPN_DEP_BUILD_KIND_SHARED,
    },
    .libs = {
      "tcc"
    },
    .build = tcc_build,
    .package = tcc_package,
  };
}
