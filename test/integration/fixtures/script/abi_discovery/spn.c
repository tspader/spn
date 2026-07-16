#include "spn.h"

#include <stdio.h>

SPN_EXPORT
s32 package(spn_t* spn, spn_node_ctx_t* ctx) {
  spn_fs_create_dir("/store/misc/data");
  spn_fs_copy_glob("/source/data/*.txt", "/store/misc/data");

  FILE* witness = fopen("/store/misc/witness", "a");
  if (!witness) {
    return 1;
  }
  fputc('R', witness);
  fclose(witness);

  return 0;
}
