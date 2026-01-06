#include "spn.h"

void configure(spn_build_ctx_t* b) {
  spn_add_system_dep(b, "m");
}
