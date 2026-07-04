#include "spum.h"

int main(int num_args, const char** args) {
  (void)num_args;
  (void)args;

  return spum_c_value() + spum_cxx_value() == 69 ? 0 : 1;
}
