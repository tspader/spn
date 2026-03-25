#include "spn.h"

void configure(spn_config_t* c) {
  spn_target_t* main = spn_add_exe(c, "main");
  spn_target_add_source(main, "main.c");
  spn_target_embed_file(main, "hello.txt");
}
