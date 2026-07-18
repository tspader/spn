#include "gen.h"

#ifndef EXPECT_GEN
#define EXPECT_GEN 1
#endif

#if GEN_VALUE != EXPECT_GEN
#error GEN_VALUE
#endif

int main() {
  return 0;
}
