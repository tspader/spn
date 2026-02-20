#include "spn.h"

void configure(spn_config_t* c) {
  spn_target_t* foo = spn_add_exe(c, "foo");
  spn_target_add_source(foo, "main.c");
}
