#include <stdio.h>
#include <tree_sitter/api.h>

int main(void) {
  TSParser *parser = ts_parser_new();
  printf("parser: %p\n", (void *) parser);
  ts_parser_delete(parser);
  return 0;
}
