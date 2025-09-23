#include <stdio.h>
#include <sds.h>
#include <sds.c>

int main(void) {
  sds value = sdsnew("hello");
  value = sdscat(value, " world");
  printf("%s\n", value);
  sdsfree(value);
  return 0;
}
