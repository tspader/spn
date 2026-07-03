#include "spum.h"

int main(int num_args, const char** args) {
  (void)num_args;
  (void)args;

  return spum_value(4761) == 69 ? 0 : 1;
}
