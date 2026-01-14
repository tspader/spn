#include "spn.h"

void configure(spn_config_t* c) {
  spn_target_t* test = spn_add_test(c, "test");
  spn_target_add_source(test, "main.c");
}
