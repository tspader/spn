#include <stdlib.h>

int main() {
  int* data = malloc(4 * sizeof(int));
  volatile int oob = data[4];
  (void)oob;
  free(data);
  return 0;
}
