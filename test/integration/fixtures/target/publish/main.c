#include "kit.h"
#include "kit/a.h"
#include "kit/b.h"

int main(int num_args, const char** args) {
  (void)num_args;
  (void)args;
  return (KIT_VALUE == 1 && KIT_A == 2 && KIT_B == 3) ? 0 : 1;
}
