#include <stdio.h>

const char* spum() {
  return "bar";
}

int main(int num_args, const char** args) {
  printf("hello, %s!\n", spum());
  return 0;
}
