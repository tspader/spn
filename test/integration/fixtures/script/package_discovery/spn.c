#include "spn.h"

#include <stdio.h>

SPN_EXPORT
s32 package(spn_t* spn, spn_node_ctx_t* ctx) {
  FILE* in = fopen("/source/data.txt", "r");
  if (!in) {
    return 1;
  }
  char data [64] = {0};
  fread(data, 1, sizeof(data) - 1, in);
  fclose(in);

  spn_fs_create_dir("/store/misc");

  FILE* out = fopen("/store/misc/data", "w");
  if (!out) {
    return 1;
  }
  fputs(data, out);
  fclose(out);

  FILE* witness = fopen("/store/misc/witness", "a");
  if (!witness) {
    return 1;
  }
  fputc('R', witness);
  fclose(witness);

  return 0;
}
