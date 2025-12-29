#include "spn.h"

void configure(spn_build_ctx_t* b) {
  spn_target_t* test = spn_add_test(b, "test");
  spn_target_add_source(test, "main.c");
}
