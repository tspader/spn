#include <stdio.h>
#include <linenoise.h>
#include <linenoise.c>

int main(void) {
  linenoiseSetMultiLine(1);
  linenoiseHistoryAdd("hello");
  linenoiseHistoryAdd("world");
  printf("linenoise ready\n");
  return 0;
}
