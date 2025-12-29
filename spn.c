#include "spn.h"

void configure(spn_build_ctx_t* build) {
  spn_target_t* target = spn_add_bin(build, "foobar");
  spn_target_add_source(target, "source/foobar.c");
  int* foo = 0;
  int bar = 10 + *foo;
}

