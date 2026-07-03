#include "spn.h"

SPN_EXPORT
spn_err_t configure(spn_t* spn) {
  spn_config_t* config = (spn_config_t*)spn;
  (void)config;
  spn_target_t* main = spn_add_exe(config, "main");
  spn_target_add_source(main, "main.c");
  spn_target_embed_file(main, "hello.txt");
  return SPN_OK;
}
