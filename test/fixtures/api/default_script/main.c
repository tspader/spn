#include "version.h"

int main(int num_args, const char** args) {
  (void)num_args;
  (void)args;
  return DEFAULT_SCRIPT_VERSION == 42 ? 0 : 1;
}
