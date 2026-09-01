#include <stdio.h>

extern int fibonacci [16];

int main(void) {
  for (int i = 0; i < 16; i++) {
    printf("%d ", fibonacci[i]);
  }
  printf("\n");
  return 0;
}
