#include "spn.h"

void configure(spn_build_ctx_t* b) {
  spn_target_t* lib = spn_get_target(b, "mylib");
  spn_log(b, lib ? "found it!" : "could not find it");
}
