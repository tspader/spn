#include "spum.h"

#include <stdio.h>

int main(void) {
  FILE* file = fopen("ran.txt", "wb");
  if (!file) {
    return 1;
  }

  fprintf(file, "%d\n", SPUM_MAGIC);
  fclose(file);
  return SPUM_MAGIC == 77 ? 0 : 1;
}
