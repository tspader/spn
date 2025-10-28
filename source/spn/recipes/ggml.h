#define SPN_PACKAGE ggml
#define SPN_OPTIONS() \
  SPN_OPTION(ggml_backend_t, backend)

#include "spn/types.h"

typedef struct {
  bool cpu;
  bool cuda;
  bool metal;
} ggml_backend_t;

#include "spn/gen.h"

void ggml_configure(spn_dep_builder_t build) {
}

void ggml_build(spn_dep_builder_t build) {
}

void ggml_package(spn_dep_builder_t build) {
  // spn_recipe_copy(&build, SPN_DIR_SOURCE, "sp.h", SPN_DIR_STORE, "");
  // spn_recipe_copy3(&build, (spn_recipe_copy_config_t []) {
  //   { { SPN_DIR_SOURCE, "sp.h" },     { SPN_DIR_INCLUDE } },
  //   { { SPN_DIR_SOURCE, "examples" }, { SPN_DIR_VENDOR } },
  // });
}

spn_recipe_info_t ggml() {
  return (spn_recipe_info_t) {
    .name = "ggml",
    .git = "ggml-org/ggml",
    .kinds = {
      SPN_DEP_BUILD_KIND_STATIC,
      SPN_DEP_BUILD_KIND_SHARED
    },
    .configure = ggml_configure,
  };
}
