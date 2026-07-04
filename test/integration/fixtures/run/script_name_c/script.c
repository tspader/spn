#include <stdio.h>

int main(void) {
  FILE* file = fopen("ran.txt", "wb");
  if (!file) {
    return 1;
  }

  fputs("script-name-c\n", file);
  fclose(file);
  return 0;
}
