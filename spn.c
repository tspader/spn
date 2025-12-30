#include "spn.h"

void configure(spn_build_ctx_t* b) {
  spn_target_t* target = spn_get_target(b, "spn");
  spn_target_embed_file(target, "doc/embed.bin");
}

