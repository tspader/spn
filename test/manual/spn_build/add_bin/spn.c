#include "spn.h"

void configure(spn_build_ctx_t* b) {
  spn_target_t* foo = spn_add_bin(b, "foo");
  spn_target_add_source(foo, "main.c");
}
