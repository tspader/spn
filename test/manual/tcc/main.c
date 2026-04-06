#include <stdio.h>
#include "libtcc.h"

int main(int num_args, const char** args) {
  TCCState* tcc = tcc_new();
  printf("hello, %s!\n", "world");
  return 0;
}
