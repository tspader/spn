#include "sp.h"

int main(void) {
  sp_str_t greeting = sp_str_lit("hello");
  return greeting.len == 5 ? 0 : 1;
}
