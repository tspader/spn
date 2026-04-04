#include "spn.h"

s32 run_make(spn_node_ctx_t* ctx) {
  spn_build_ctx_t* dep = spn_node_ctx_get_build(ctx);
  spn_make_t* make = spn_make_new(dep);
  spn_linkage_t linkage = spn_get_linkage(dep);

  spn_copy(dep, SPN_DIR_SOURCE, "Makefile", SPN_DIR_WORK, "");
  spn_copy(dep, SPN_DIR_SOURCE, "spum.c", SPN_DIR_WORK, "");
  spn_copy(dep, SPN_DIR_SOURCE, "spum.h", SPN_DIR_WORK, "");

  switch (linkage) {
    case SPN_LIB_KIND_STATIC: {
      spn_make_add_target(make, "static");
      break;
    }
    case SPN_LIB_KIND_SHARED: {
      spn_make_add_target(make, "shared");
      break;
    }
    case SPN_LIB_KIND_SOURCE: {
      break;
    }
  }

  return spn_make_run(make);
}

void configure(spn_build_ctx_t* dep) {
  spn_node_t* make = spn_add_node(dep, "make");
  spn_node_set_fn(make, run_make);
}

void package(spn_build_ctx_t* dep) {
  spn_linkage_t linkage = spn_get_linkage(dep);

  switch (linkage) {
    case SPN_LIB_KIND_STATIC: {
      spn_copy(dep, SPN_DIR_WORK, "libspum.a", SPN_DIR_LIB, "");
      break;
    }
    case SPN_LIB_KIND_SHARED: {
      spn_copy(dep, SPN_DIR_WORK, "libspum.so", SPN_DIR_LIB, "");
      break;
    }
    case SPN_LIB_KIND_SOURCE: {
      break;
    }
  }

  spn_copy(dep, SPN_DIR_SOURCE, "spum.h", SPN_DIR_INCLUDE, "");
}
